# 第三方组件

- `components/board_port` 的显示与触摸适配、`esp_lcd_st7796`、`esp_lcd_touch_ft6336`：来自 Waveshare 官方 ESP32-S3-Touch-LCD-3.5 的 ESP-IDF/02_lvgl_example。原文件中的许可声明保留。上游固定提交 `283ec84c566c096f8c30493b93dcd4b0bb608de7`，Apache-2.0，完整许可见 `licenses/Waveshare-Apache-2.0.txt`。适配修改包括屏幕 SPI 降为 40 MHz、复位等待延长至 150 ms，并保留厂家冷上电的软件重启步骤；外设初始化结束后另行复位和重绘屏幕。
- `components/esp-nimble-cpp`：h2zero/esp-nimble-cpp，v2.3.4，Apache-2.0；目录中保留 LICENSE。
- TinyUSB USB 描述符及 UAC 控制布局参考 hathach/tinyusb 的 audio_test 示例（0.18.0），MIT。依赖本体通过 Espressif 组件管理器取得，许可见组件 LICENSE。
- ESP-IDF、esp_lvgl_port、esp_codec_dev、esp_lcd_touch、esp_io_expander 及音频 Opus 解码库：Espressif 官方组件；许可依各组件随附文件，其中 esp_audio_codec 含仅允许配合 Espressif 芯片使用的限制。
- LVGL 8.3.11：MIT。
- `main/font_cn20.c` 为 Noto Sans SC 派生的 SiriPadCJK 字形子集，SIL OFL 1.1；许可及版权声明见 `licenses/SiriPadCJK-OFL.txt`。源自 https://github.com/google/fonts/tree/main/ofl/notosanssc ，使用 fontTools 固定字重 500，再由 lv_font_conv 转换为 20 px / 4 bpp / LVGL 格式。公开版本不包含此前本地使用的 STHeiti 字形。源字体 SHA-256：`a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`。
