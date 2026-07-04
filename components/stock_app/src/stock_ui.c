#include "stock_ui.h"
#include "stock_data.h"
#include "stock_config.h"
#include "wifi_sta.h"

#include <stdio.h>
#include <string.h>
#include "bsp_lvgl.h"
#include "lvgl.h"

LV_FONT_DECLARE(font_stock_16);

static lv_obj_t *s_price;
static lv_obj_t *s_change;
static lv_obj_t *s_open;
static lv_obj_t *s_prev;
static lv_obj_t *s_high;
static lv_obj_t *s_low;
static lv_obj_t *s_time;
static lv_obj_t *s_status;

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &font_stock_16, 0);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    return l;
}

esp_err_t stock_ui_create(void)
{
    bsp_lvgl_lock();

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);

    make_label(scr, STOCK_NAME_UTF8 "  " STOCK_CODE, 10, 8);

    s_price = lv_label_create(scr);
    lv_obj_set_style_text_font(s_price, &font_stock_16, 0);
    lv_label_set_text(s_price, "--");
    lv_obj_set_pos(s_price, 10, 40);

    s_change = make_label(scr, "涨跌 --", 10, 72);

    s_open = make_label(scr, "今开 --", 10, 108);
    s_prev = make_label(scr, "昨收 --", 200, 108);
    s_high = make_label(scr, "最高 --", 10, 140);
    s_low  = make_label(scr, "最低 --", 200, 140);

    s_time   = make_label(scr, "更新 --", 10, 176);
    s_status = make_label(scr, "连接中", 10, 208);

    bsp_lvgl_unlock();
    return ESP_OK;
}

void stock_ui_refresh(void)
{
    stock_quote_t q;
    stock_data_get(&q);
    wifi_sta_status_t wifi = wifi_sta_get_status();

    char buf[48];

    bsp_lvgl_lock();

    if (!q.valid) {
        lv_label_set_text(s_price, "--");
        lv_label_set_text(s_change, "涨跌 --");
        lv_label_set_text(s_open, "今开 --");
        lv_label_set_text(s_prev, "昨收 --");
        lv_label_set_text(s_high, "最高 --");
        lv_label_set_text(s_low, "最低 --");
        lv_label_set_text(s_time, "更新 --");
    } else {
        snprintf(buf, sizeof(buf), "%.2f", q.price);
        lv_label_set_text(s_price, buf);

        const char *arrow = q.change_pct >= 0.0f ? "↑" : "↓";
        snprintf(buf, sizeof(buf), "涨跌 %s%.2f (%.2f%%)", arrow, q.change_amount, q.change_pct);
        lv_label_set_text(s_change, buf);

        snprintf(buf, sizeof(buf), "今开 %.2f", q.open);
        lv_label_set_text(s_open, buf);
        snprintf(buf, sizeof(buf), "昨收 %.2f", q.prev_close);
        lv_label_set_text(s_prev, buf);
        snprintf(buf, sizeof(buf), "最高 %.2f", q.high);
        lv_label_set_text(s_high, buf);
        snprintf(buf, sizeof(buf), "最低 %.2f", q.low);
        lv_label_set_text(s_low, buf);

        if (strlen(q.update_time) == 14) {
            snprintf(buf, sizeof(buf), "更新 %c%c:%c%c:%c%c",
                     q.update_time[8], q.update_time[9],
                     q.update_time[10], q.update_time[11],
                     q.update_time[12], q.update_time[13]);
        } else {
            snprintf(buf, sizeof(buf), "更新 %s", q.update_time);
        }
        lv_label_set_text(s_time, buf);
    }

    if (wifi == WIFI_STA_STATUS_CONNECTED) {
        lv_label_set_text(s_status, q.valid ? "" : "无数据");
    } else {
        lv_label_set_text(s_status, "连接中");
    }

    bsp_lvgl_unlock();
}
