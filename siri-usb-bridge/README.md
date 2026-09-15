> 历史树莓派原型：用户实机遇到供电不足，当前转向 [ESP32 触屏方案](../esp32-siri-pad/README.md)。下述机器状态是原测试时点记录，并非当前在线状态；不要自动执行部署步骤。

# 老四 Siri Remote USB 键盘与麦克风原型

目标：Siri Remote A2854 经蓝牙连接老四，老四通过板载 USB-C 向一台 Mac 提供 HID 键盘和 USB Audio Class 2 麦克风。输入和声音不经过局域网；SSH 仅用于安装维护。

本目录是开发原型，未完成实物配对和 Mac USB 验收。USB gadget 描述符已设置开机自启；蓝牙接收、按键和录音转发尚未启动，也未设置自启。

## 当前映射

- Siri：保持左 Option；音频来自遥控器自身麦克风，由 Opus 解码成 48 kHz 单声道 PCM。
- TV：Return。
- 电源：Control + Command + Q（Mac 锁屏）。
- 返回：Escape。此原型尚未加入原 Mac 软件的 Codex / Chrome / Claude 定向切换。
- 方向环：方向键。音量、播放、静音和触控暂不转发。

程序启动或 USB/BLE 重新连接后，先按下并松开一次任意按键，收到全松开状态后才接受新按键，防止旧按键在新连接中重放。按键在连续两秒收不到任何按键/音频报告时自动释放；长按方向键等无持续报告的动作会受此保护限制，需要现场调整。

## 准备状态

- 老四：Debian 13、内核 6.12.47+rpt-rpi-2712；内置蓝牙可用。
- USB 键盘与 UAC2 内核模块可用，USB-C 控制器 `1000480000.usb` 已启用；截至 2026-09-14 22:01 状态为 `not attached`，Mac 数据连接尚未建立。
- 上游源码固定版本与许可证见 UPSTREAM.md，未修改其内容。
- scripts/gadget.py 创建独立 siri_usb 配置；prepare 只校验配置，不绑定控制器。
- scripts/bridge.py 读取上游 events 输出。原始音频仅经过内存，不写日志或录音文件。
- 构建依赖安装自 Debian / Raspberry Pi 官方软件源，Rust 工具链通过发行版 rustup 从官方源安装。
- 老四 release 构建成功。上游测试 81 通过、2 因缺少 microphone-dump.txt 真实录音样本失败、1 硬件测试忽略；本项目自己的 8 项检查在老四通过。尚不能据此确认遥控器实物录音效果。

## 后续实施顺序

1. 确认 USB-C 数据线、目标 Mac 和老四供电。不要用 USB-A 对 USB-A 线连接两个主机端口；不要随意并联两个电源。
2. 已备份 /boot/firmware/config.txt 至 `~/workspace-agent/backup/siri-usb-before-20260914-215715/config.txt`，添加 `dtoverlay=dwc2,dr_mode=peripheral` 并重启。
3. 检查 `/sys/class/udc` 出现设备控制器，确认没有其他已绑定 gadget。
4. 当前由 `siri-usb-gadget.service` 管理描述符，不要同时手动运行 start。验证 Mac 同时枚举键盘和麦克风；该服务本身不发送按键。已使用 WirePlumber 精确设备规则将 gadget 卡留给桥接程序，正常 HDMI 输出保留。
5. 将遥控器放到老四旁边，进入配对模式，再使用固定上游接收器的 pair 命令。配对会影响其原 Mac 连接。
6. 上游若被 BlueZ 的 HID 插件占用，需要评估并备份 bluetooth.service 后禁用 input/hog 插件。此变更会影响同一蓝牙控制器上的其他键鼠，当前未执行。
7. 确认配对后的稳定地址和 UAC2 ALSA 卡名，以参数启动 bridge.py。Mac 选择本 USB 麦克风，微信输入法保持长按左 Option 触发语音输入。
8. 实测：按下/松开、USB 拔插、蓝牙断连、录音声音和电源稳定性；通过后再配置权限及自启服务。

## 测试

```sh
python3 -m unittest discover -s tests -v
```

包含真实 libopus 静音帧解码（Linux）、按键报告、按键释放、过期队列清理、音频包边界检查。它们不替代实体 USB 和遥控器测试。

## 停止

先停止 bridge.py，再运行 `sudo systemctl disable --now siri-usb-gadget` 解绑并停止本项目配置自启。服务使用 `/opt/siri-usb-bridge/gadget.py` 的 root 所有副本。启动配置可使用上述备份还原；恢复 WirePlumber 自动管理 gadget 卡时移除 `/etc/wireplumber/wireplumber.conf.d/60-siri-usb-exclusive.conf` 并重启用户 WirePlumber 服务。蓝牙服务尚未修改。USB VID/PID 仅供本地原型使用，不用于公开销售的产品。
