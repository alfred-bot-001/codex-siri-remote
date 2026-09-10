# 简体中文版与微信输入法配置

本 fork 汉化菜单栏、参数调节、按键配置编辑器和状态提示。配置动作标识保持兼容。

## 构建

在仓库根目录运行：

```sh
cd app
./build.sh
HYPERVIBE_VERSION=1.0.1 HYPERVIBE_BUILD_NUMBER=2026091001 ./create_app_bundle.sh
```

退出正在运行的 HyperVibe 后，将生成的 `app/HyperVibe.app` 放入“应用程序”文件夹。
重新编译可能改变本地签名；如果按键失效，在系统设置 → 隐私与安全性中，重新添加此应用的“输入监控”和“辅助功能”权限，然后重新启动。以实际运行状态为准，旧条目的开关开启不一定代表新签名已授权。

## 微信输入法与 Claude

推荐配置为 [`../examples/wechat-claude.jsonc`](../examples/wechat-claude.jsonc)。先备份自己的 `~/.config/siriremote/config.jsonc`，再将此示例复制到该位置。

- Siri 语音键：按下时保持左 Option，松开时释放；微信输入法需已配置长按左 Option 开始语音输入。
- 返回键：Codex → Chrome → Claude → Codex；其他应用切回 Codex。
- TV 键：单击 Return，双击触发原有中断动作（双 Escape）。实际发送行为取决于目标应用的回车设置。
- Claude 继承全局语音、方向键与 Return 映射。使用前先选中目标输入框。
- 自动聚焦关闭，避免移动光标时意外切换前台应用。

此配置不启用遥控器内置麦克风；语音由微信输入法使用 Mac 当前可用的录音设备采集。

## 自动聚焦修复与验证

只有遥控器发出的新光标移动可以触发一次停留聚焦。启动、普通鼠标、陈旧位置、拖拽及等待期间切换应用不会触发；覆盖率按窗口实际可见面积计算。系统拒绝激活时不会强制置前。

在仓库根目录运行：

```sh
./tests/run-focus-verification.sh
./tests/run-software-verification.sh
```

自动聚焦专项包含 33 项检查。物理按键、输入法录音和多屏交互仍需现场验证。
