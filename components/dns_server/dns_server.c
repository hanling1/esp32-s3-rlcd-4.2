/* Captive-portal DNS hijack. Ported from
 * https://github.com/zhy345517-rgb/esp32_wifi_configuration (matches the
 * Espressif esp-idf captive_portal example). Reused for personal/learning use. */
#include "dns_server.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"

#define DNS_PORT 53
#define DNS_MAX_PACKET_LEN 512
#define DNS_TASK_STACK_SIZE 4096
#define DNS_TASK_PRIORITY 5

#define DNS_FLAG_QR (1U << 15)
#define DNS_FLAG_OPCODE_MASK 0x7800
#define DNS_QTYPE_A 0x0001
#define DNS_ANSWER_TTL_SEC 300

static const char *TAG = "dns_server";

typedef struct __attribute__((__packed__)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

typedef struct __attribute__((__packed__)) {
    uint16_t type;
    uint16_t class;
} dns_question_t;

typedef struct __attribute__((__packed__)) {
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;

struct dns_server_handle {
    bool started;
    TaskHandle_t task;
    int sock;
    int num_entries;
    dns_entry_pair_t entries[];
};

static bool dns_name_parse(const uint8_t *packet,
                           size_t packet_len,
                           size_t *offset,
                           char *name,
                           size_t name_size)
{
    size_t in = *offset;
    size_t out = 0;

    if (name_size == 0) {
        return false;
    }

    while (in < packet_len) {
        uint8_t label_len = packet[in++];
        if (label_len == 0) {
            if (out == 0) {
                name[out++] = '.';
            }
            name[out - 1] = '\0';
            *offset = in;
            return true;
        }

        if ((label_len & 0xC0U) != 0) {
            return false;
        }

        if (in + label_len > packet_len) {
            return false;
        }

        if (out + label_len + 1 >= name_size) {
            return false;
        }

        memcpy(&name[out], &packet[in], label_len);
        out += label_len;
        name[out++] = '.';
        in += label_len;
    }

    return false;
}

static bool dns_pick_ip(const dns_server_handle_t handle, const char *query_name, esp_ip4_addr_t *out_ip)
{
    for (int i = 0; i < handle->num_entries; ++i) {
        const dns_entry_pair_t *entry = &handle->entries[i];
        if (strcmp(entry->name, "*") != 0 && strcmp(entry->name, query_name) != 0) {
            continue;
        }

        if (entry->if_key != NULL) {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey(entry->if_key);
            if (netif != NULL) {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != IPADDR_ANY) {
                    out_ip->addr = ip_info.ip.addr;
                    return true;
                }
            }
        } else if (entry->ip.addr != IPADDR_ANY) {
            out_ip->addr = entry->ip.addr;
            return true;
        }
    }

    return false;
}

static int dns_make_reply(const uint8_t *req,
                          size_t req_len,
                          uint8_t *reply,
                          size_t reply_cap,
                          const dns_server_handle_t handle)
{
    if (req_len < sizeof(dns_header_t) || req_len > reply_cap) {
        return -1;
    }

    memcpy(reply, req, req_len);

    dns_header_t *header = (dns_header_t *)reply;
    if ((ntohs(header->flags) & DNS_FLAG_OPCODE_MASK) != 0) {
        return 0;
    }

    uint16_t qd_count = ntohs(header->qd_count);
    uint16_t an_count = 0;
    header->flags = htons(ntohs(header->flags) | DNS_FLAG_QR);

    size_t qd_offset = sizeof(dns_header_t);
    size_t answer_offset = req_len;
    char domain[128];

    for (uint16_t i = 0; i < qd_count; ++i) {
        size_t name_offset = qd_offset;
        if (!dns_name_parse(req, req_len, &qd_offset, domain, sizeof(domain))) {
            ESP_LOGW(TAG, "failed to parse question");
            return -1;
        }

        if (qd_offset + sizeof(dns_question_t) > req_len) {
            return -1;
        }

        const dns_question_t *question = (const dns_question_t *)&req[qd_offset];
        qd_offset += sizeof(dns_question_t);

        if (ntohs(question->type) != DNS_QTYPE_A) {
            continue;
        }

        esp_ip4_addr_t selected_ip = {.addr = IPADDR_ANY};
        if (!dns_pick_ip(handle, domain, &selected_ip)) {
            continue;
        }

        if (answer_offset + sizeof(dns_answer_t) > reply_cap) {
            return -1;
        }

        dns_answer_t *answer = (dns_answer_t *)&reply[answer_offset];
        answer->ptr_offset = htons((uint16_t)(0xC000U | (name_offset & 0x3FFFU)));
        answer->type = htons(DNS_QTYPE_A);
        answer->class = htons(ntohs(question->class));
        answer->ttl = htonl(DNS_ANSWER_TTL_SEC);
        answer->addr_len = htons(sizeof(uint32_t));
        answer->ip_addr = selected_ip.addr;

        answer_offset += sizeof(dns_answer_t);
        an_count++;
    }

    header->an_count = htons(an_count);
    return (int)answer_offset;
}

static void dns_server_task(void *arg)
{
    dns_server_handle_t handle = (dns_server_handle_t)arg;
    uint8_t rx_buf[DNS_MAX_PACKET_LEN];
    uint8_t tx_buf[DNS_MAX_PACKET_LEN];

    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(DNS_PORT);

    handle->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle->sock < 0) {
        ESP_LOGE(TAG, "socket create failed: errno=%d", errno);
        handle->task = NULL;
        vTaskDelete(NULL);
        return;
    }

    struct timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    setsockopt(handle->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (bind(handle->sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        ESP_LOGE(TAG, "bind failed: errno=%d", errno);
        close(handle->sock);
        handle->sock = -1;
        handle->task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "dns server started on :%d", DNS_PORT);

    while (handle->started) {
        struct sockaddr_storage source_addr;
        socklen_t source_len = sizeof(source_addr);
        int rx_len = recvfrom(handle->sock,
                              rx_buf,
                              sizeof(rx_buf),
                              0,
                              (struct sockaddr *)&source_addr,
                              &source_len);

        if (rx_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            ESP_LOGW(TAG, "recvfrom error: errno=%d", errno);
            break;
        }

        int tx_len = dns_make_reply(rx_buf, (size_t)rx_len, tx_buf, sizeof(tx_buf), handle);
        if (tx_len <= 0) {
            continue;
        }

        int sent = sendto(handle->sock,
                          tx_buf,
                          (size_t)tx_len,
                          0,
                          (struct sockaddr *)&source_addr,
                          source_len);
        if (sent < 0) {
            ESP_LOGW(TAG, "sendto failed: errno=%d", errno);
        }
    }

    if (handle->sock >= 0) {
        shutdown(handle->sock, SHUT_RDWR);
        close(handle->sock);
        handle->sock = -1;
    }

    ESP_LOGI(TAG, "dns server stopped");
    handle->task = NULL;
    vTaskDelete(NULL);
}

dns_server_handle_t dns_server_start(const dns_server_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, NULL, TAG, "config is null");
    ESP_RETURN_ON_FALSE(config->num_of_entries > 0, NULL, TAG, "invalid entry count");

    size_t alloc_size = sizeof(struct dns_server_handle) +
                        (size_t)config->num_of_entries * sizeof(dns_entry_pair_t);
    dns_server_handle_t handle = (dns_server_handle_t)calloc(1, alloc_size);
    ESP_RETURN_ON_FALSE(handle != NULL, NULL, TAG, "no memory");

    handle->started = true;
    handle->sock = -1;
    handle->num_entries = config->num_of_entries;
    memcpy(handle->entries,
           config->item,
           (size_t)config->num_of_entries * sizeof(dns_entry_pair_t));

    BaseType_t task_ok = xTaskCreate(dns_server_task,
                                     "dns_server",
                                     DNS_TASK_STACK_SIZE,
                                     handle,
                                     DNS_TASK_PRIORITY,
                                     &handle->task);
    if (task_ok != pdPASS) {
        free(handle);
        ESP_LOGE(TAG, "failed to create task");
        return NULL;
    }

    return handle;
}

void dns_server_stop(dns_server_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    handle->started = false;

    if (handle->sock >= 0) {
        shutdown(handle->sock, SHUT_RDWR);
        close(handle->sock);
        handle->sock = -1;
    }

    for (int i = 0; i < 20 && handle->task != NULL; ++i) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (handle->task != NULL) {
        vTaskDelete(handle->task);
        handle->task = NULL;
    }

    free(handle);
}
