> 历史验证程序：当前触屏 USB 键盘与麦克风固件见 [esp32-siri-pad](../esp32-siri-pad/README.md)。本目录保留原测试时点的记录，不代表设备当前运行版本。

# Siri Remote → ESP32-S3 蓝牙连接验证

设备：Waveshare ESP32-S3-Touch-LCD-3.5，实测 ESP32-S3 rev 0.2、8MB PSRAM、16MB flash。

build 1–2 验证 BLE 发现、加密配对、HID 服务和按键通知。build 3 增加限时麦克风数据采集，经 USB 串口传到本机，由本机 libopus 解码为 WAV；尚未实现 ESP32 内部 Opus 解码或标准 USB Audio Class 麦克风。没有 USB 键盘输出。屏幕未初始化，刷入后原来的界面不再运行。

当前 build 4 请求 15ms BLE 连接间隔；一次 5.74 秒实际语音测试 287 帧解码成功、无缺包。详见《麦克风测试记录.md》。当前保留麦克风测试固件，采集窗口已关闭。

音频命令：`m` 在已连接时开启最多 60 秒的接收窗口，`x` 停止并清空队列。只有窗口内的 FA 数据才送入 64 帧队列，记录丢帧数；断线时关闭窗口。`capture_mic.py` 将单段测试保存到 recordings，仅本机保存，不上传，最多 30 秒音频或等待 65 秒，音频结束后自动关闭采集。按语音键实际启动/停止遥控器发声数据。

串口命令：`s` 开始 120 秒扫描，`d` 停止扫描并断开连接。扫描只选择附近（RSSI ≥ -60）的 Apple HID 配对广播，首次选择后本次运行锁定同一身份。NimBLE 的绑定保存在 NVS。本次诊断固件 build 2 仅自动恢复已实测遥控器 28:2D:7F:3F:50:6E 的绑定；断线后最多尝试 5 次重连，不自动选择其他设备。首次配对需要遥控器在板子旁进入配对模式，重连只需普通按键唤醒。

按键证据必须是 `BUTTON mask=...` 随实际按下、松开发生变化；仅 `SIRI_FOUND` 不等于连接成功，仅 `LINK_CONNECTED` 不等于完成配对。

编译使用隔离工具目录：

```sh
PLATFORMIO_CORE_DIR=/Users/yafei/my-agents/.tools/platformio /Users/yafei/my-agents/.tools/esp32-test/bin/pio run --project-dir /Users/yafei/my-agents/esp32-siri-test
```

原固件完整备份保存在 backups 目录（含原私有配置，不上传）。恢复时确认同一块板且没有串口监控占用，再用 esptool 从地址 0 写入原 16MB 镜像。恢复会覆盖测试固件及测试绑定；不烧写 eFuse。

协议参考：[azais-corentin/siri-remote](https://github.com/azais-corentin/siri-remote) 的 Apple HID 广播格式、报告引用描述符与 0xAF 输入启用逻辑。BLE 使用固定版本 NimBLE-Arduino 2.3.6，PlatformIO espressif32 6.12.0。
