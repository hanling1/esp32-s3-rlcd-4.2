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
