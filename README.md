# ESP32-S3-RLCD-4.2 底座

面向创意应用开发的可复用固件底座。屏幕点亮链路已验证通过,应用开发者基于本底座只需关注 UI 与业务逻辑。

## 硬件与工具链

| 项 | 规格 |
|---|---|
| 芯片 | ESP32-S3-WROOM-1-N16R8(16MB flash + 8MB 八线 PSRAM@80MHz) |
| 屏幕 | 4.2" 反射式单色 LCD,ST7305 控制器,SPI,400×300 横屏,1-bit 黑白 |
| 输入 | 3 个物理按键 BOOT / PWR / KEY(无触摸) |
| 框架 | ESP-IDF v6.0.2 + LVGL v8.3.11 |

## 快速开始

环境激活（每个新 shell 都要执行）：

```bash
source $HOME/.espressif/v6.0.2/esp-idf/export.sh
# GitHub 下载慢时启用代理
export http_proxy=http://127.0.0.1:7890 https_proxy=http://127.0.0.1:7890 no_proxy=localhost,127.0.0.1
```

编译 / 烧录 / 监视：

```bash
cd /Users/hanling/Documents/code/esp32-s3
idf.py build
idf.py -p /dev/cu.usbmodem21401 flash    # 端口号可能变，消失就重插 USB
idf.py -p /dev/cu.usbmodem21401 monitor
```

## 目录结构

```
esp32-s3/
├── CMakeLists.txt              项目根
├── sdkconfig.defaults          关键配置（target / PSRAM / LVGL 色深）已锁定
├── main/
│   ├── main.c                  app_main: bsp_init → bsp_lvgl_init → 画 UI
│   └── CMakeLists.txt          REQUIRES: bsp_rlcd42, lvgl
└── components/bsp_rlcd42/      可复用底座组件
    ├── include/                公共 API（应用只需 include 这些）
    │   ├── bsp_rlcd42.h          bsp_init()
    │   ├── bsp_rlcd42_pins.h     所有 GPIO 常量（单一真相源）
    │   ├── bsp_lvgl.h            LVGL 生命周期 + 线程锁
    │   └── bsp_buttons.h         按键事件 API
    └── src/                    实现
        ├── st7305.c/.h           ST7305 驱动（SPI + init 序列 + LUT 打包）
        ├── bsp_lvgl.c            LVGL 集成（flush / tick / task）
        ├── bsp_buttons.c         按键去抖
        └── bsp_rlcd42.c          bsp_init() 组合
```

## 应用开发 API

写新应用时几乎只需要这 5 个函数加 LVGL。

```c
#include "bsp_rlcd42.h"
#include "bsp_lvgl.h"
#include "bsp_buttons.h"
#include "lvgl.h"

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_init());        // 初始化屏幕驱动 + 按键
    ESP_ERROR_CHECK(bsp_lvgl_init());   // 启动 LVGL（内部起后台渲染 task）

    bsp_lvgl_lock();
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Your App");
    lv_obj_center(label);
    bsp_lvgl_unlock();

    while (1) {
        if (bsp_button_get_pressed_event(BSP_BTN_KEY)) {
            bsp_lvgl_lock();
            /* 更新 UI */
            bsp_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

| 函数 | 作用 |
|---|---|
| `bsp_init()` | 初始化 ST7305 屏幕 + 按键 GPIO |
| `bsp_lvgl_init()` | 启动 LVGL，注册显示驱动，起后台渲染 task（core 0） |
| `bsp_lvgl_lock()` / `bsp_lvgl_unlock()` | 任何 LVGL 操作前后必须加锁 |
| `bsp_button_get_pressed_event(btn)` | 轮询，单次按下返回一次 `true`，已去抖（20ms） |

按键枚举：`BSP_BTN_BOOT` / `BSP_BTN_PWR` / `BSP_BTN_KEY`

## 铁律

1. **LVGL 非线程安全**：所有 `lv_*` 调用必须包在 `bsp_lvgl_lock()` / `bsp_lvgl_unlock()` 之间。后台渲染 task 会并发访问。
2. **颜色是伪彩色**：LVGL 跑 RGB565（16-bit），flush 时按 `亮度 < 0x7fff ? 黑 : 白` 二值化。可用 LVGL 全部彩色 API，但屏上只有黑 / 白两级，无灰度。UI 用纯黑 / 纯白设计。
3. **改引脚只改一处**：全部 GPIO 定义在 `bsp_rlcd42_pins.h`。

   ```
   屏:  SCLK=11  MOSI=12  CS=40  DC=5  RST=41
   按键: BOOT=0   PWR=46   KEY=4
   ```

   注意：这些引脚为社区值，尚未对照官方原理图逐一核实。目前实测能点亮，正式产品前应核对（见"欠账"）。
4. **别动这几项 sdkconfig**（动了要么编不过要么 boot 循环）：

   ```
   CONFIG_IDF_TARGET="esp32s3"
   CONFIG_SPIRAM_MODE_OCT=y  +  CONFIG_SPIRAM_SPEED_80M=y   八线 PSRAM
   CONFIG_LV_COLOR_DEPTH_16=y                                非 1
   ```
5. **大 buffer 放 PSRAM**：LVGL 双缓冲（2×240KB）与 LUT（360KB）都在 PSRAM。应用中大数组也尽量用 `MALLOC_CAP_SPIRAM`。

## ST7305 底层接口

如需绕过 LVGL 做自定义绘制，`st7305.h` 暴露直接接口：

```c
st7305_set_pixel(x, y, ST7305_BLACK /* 或 ST7305_WHITE */);  // 打点
st7305_color_clear(ST7305_WHITE);                             // 清屏
st7305_display();                                              // 推送整帧到屏
st7305_draw_test_pattern();                                    // 棋盘格自检（尚未真机验证）
```

ST7305 每字节对应一个 2×2 像素块的非常规排布，通过预计算 LUT（400×300 查表）映射；init 序列从 Waveshare 官方逐字节移植。此处最易出错，已验证可用，建议不要改动。

## 设计决策

采用 **RGB565（LV_COLOR_DEPTH 16）+ LUT flush** 而非单色 `LV_COLOR_DEPTH 1`。原因：ST7305 的 2×2 块像素排布与 LVGL 通用单色 `set_px_cb` 路径不兼容，官方 RGB565 + 逐像素 LUT 映射是唯一经硬件验证的方案。代价是显存占用较大（PSRAM 充足），换取一次点亮成功。

## 当前状态与欠账

已验证：编译通过、烧录、启动无崩溃、屏幕显示文字，PSRAM / LVGL / ST7305 init 全部成功。

未完成的收尾（不影响开发新应用）：

```
[ ] 引脚对照官方原理图核实
[ ] test-pattern 棋盘格真机验证
[ ] 按键单击 → 单事件 真机验证（去抖逻辑未实测）
[ ] 按键联动 UI 变化 真机验证
```

其中按键去抖尚未在硬件上验证。若应用重度依赖按键，请优先实测。

## 创意应用起点建议

- **纯显示类**（时钟 / 天气 / 仪表盘）：直接用 LVGL，`lock` 内绘制，定时刷新。最顺路径。
- **联网类**（天气 / MQTT / OTA）：需新增 Wi-Fi + HTTP，超出当前底座范围，建议开新的功能分支并补充 Wi-Fi 相关 sdkconfig。
- **交互类**：先补按键去抖真机验证。
- **动画 / 游戏**：全屏刷新每帧推 15KB SPI@10MHz，帧率有限；高帧率需局部刷新或提高 SPI 时钟。

参考代码：Waveshare 官方仓库 `ESP32-S3-RLCD-4.2` 的 `02_Example/ESP-IDF/` 下有 WiFi / I2C / SD / Audio / RTC 等 11 个示例，开发新功能时可优先参考。

## 示例应用：股票实时行情（stock_app）

当前 `main/main.c` 运行一个单股行情查看器，展示"联网 → 取数 → 单色屏渲染"的完整链路。

- **股票**：002859 洁美科技（写死，中文名为 UTF-8 常量）
- **数据源**：腾讯 `http://qt.gtimg.cn/q=sz002859`，纯 HTTP，返回 `~` 分隔文本，无需 TLS / key / JSON 库
- **刷新**：后台 task 每 5 秒取一次；UI 每秒重绘
- **Wi-Fi**：STA 写死 `solaso_5G`（凭据在 `components/stock_app/include/stock_config.h`），断线自动重连
- **字库**：`font_stock_16`（16px 1-bpp 子集，仅嵌入 UI 用到的固定字形），由 `lv_font_conv` 生成
- **容错**：取数 / 解析失败保留上次数值、不崩溃；首次连网前显示"连接中"、无数据显示 `--`
- **涨跌**：单色屏无红绿，用 ↑ / ↓ 表示

组件结构（`components/stock_app/`）：

```
include/
  stock_config.h    写死项单一真相源（SSID/密码/股票代码/URL/刷新周期）
  wifi_sta.h        Wi-Fi STA：wifi_sta_start / wifi_sta_get_status
  stock_data.h      取数：stock_data_start / stock_data_get(stock_quote_t*)
  stock_ui.h        界面：stock_ui_create / stock_ui_refresh
src/
  wifi_sta.c        NVS 初始化 + STA 连接 + 自动重连（事件回调）
  stock_data.c      esp_http_client GET + 防御式 ~ 解析 + 5s 轮询 task
  stock_ui.c        LVGL 布局 + 刷新（内部加 bsp_lvgl_lock）
  font_stock_16.c   生成的 1-bpp 子集中文字库（勿手改，改字表须重新生成）
```

腾讯字段位置（对 `~` 切分、跳过开头 `"` 后按 0 起索引）：
`[3]现价 [4]昨收 [5]今开 [30]时间(YYYYMMDDhhmmss) [31]涨跌额 [32]涨跌% [33]最高 [34]最低`。

重新生成字库（新增字形时）：

```bash
npx lv_font_conv --font "/System/Library/Fonts/Supplemental/Arial Unicode.ttf" \
  --size 16 --bpp 1 --format lvgl --no-compress --lv-include lvgl.h \
  --range 0x20-0x7E --symbols "洁美科技现价涨跌今开昨收最高低更新连接中无数据↑↓" \
  -o components/stock_app/src/font_stock_16.c
```

**已后置到 v2**：Wi-Fi 配网（SoftAP / 二维码 / NVS 存储 / 长按重置）、多股列表、分时图。当前凭据写死在 `stock_config.h`，换网络需改此文件重新编译。交易时段外接口返回收盘价为静态值，盘中才随 5 秒刷新变化（更新时间字段可辨别）。

