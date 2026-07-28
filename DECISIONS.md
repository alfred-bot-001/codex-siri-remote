# Codex Remote 决策记录

本文件是 Codex Remote 的需求与行为真相源。实现、测试和后续评审均应先以本文件为准。

## 维护规则

- 决策状态仅使用 `Accepted`、`Assumed`、`Pending verification`、`Superseded`。
- 不静默覆盖已经接受的决定。改变决定时，将旧条目标为 `Superseded`，新增替代条目，并在文末追加变更记录。
- 静态代码、编译结果和真实硬件验证必须分开描述；未完成 A2854 实机验证前，不得把项目标记为整体完成。
- 如果新增或修改 Codex 专用热键，应先在本文件记录准确的 Codex command、键位和冲突检查结果，再修改配置或源码。

## 当前决策

### D-001 目标硬件

- **状态：** Accepted
- **决定：** 目标硬件为第三代 Siri Remote A2854 USB-C；设备已经购买并连接到本机。
- **原因：** 该型号具备触控圆盘、实体方向环和足够的独立按键。
- **影响：** 可以进入 A2854 分阶段实机验收；尚未执行的项目仍不得视为通过。
- **证据或验证：** 2026-07-28 macOS 蓝牙系统数据确认 Apple Vendor ID `0x004C`、Product ID `0x0315`、固件 `0x0021` 和 BLE 连接；上游检测器按 Apple HID 接口枚举设备，见 [RemoteDetector.swift](app/RemoteDetector.swift#L51-L88)。

### D-002 实现底座与参考项目

- **状态：** Accepted
- **决定：** 以 [SiriRemoteForge](https://github.com/HOLODATA-COM/SiriRemoteForge) 为实现底座；[HyperVibe](https://github.com/machinarii/hypervibe) 和 [VibePad](https://github.com/ignatovv/VibePad) 只作为交互设计参考。
- **原因：** SiriRemoteForge 已包含 A2854 所需的 HID、触控、鼠标、动作执行和按应用配置能力。
- **影响：** 不从 HyperVibe 或 VibePad 引入设备层，也不把游戏手柄支持加入 V1。
- **证据或验证：** 上游设备检测入口见 [RemoteDetector.swift](app/RemoteDetector.swift#L51-L116)，中心键按 workflow/原生鼠标分流入口见 [RemoteInputHandler.swift](app/RemoteInputHandler.swift#L690-L700)。

### D-003 轨道优先级

- **状态：** Accepted
- **决定：** V1 优先完成轨道 A；轨道 B 只保留“遥控器工作流意图 → 执行器”接口。
- **原因：** 轨道 A 无需修改 Codex，能够先形成稳定的本机控制闭环。
- **影响：** V1 不实现 Codex 状态订阅、审批 RPC、原生重试或 app-server 客户端。
- **证据或验证：** 代码中应只有 macOS 工作流执行器，不应出现 Codex app-server 协议依赖或伪状态机。

### D-004 Codex 集成边界

- **状态：** Accepted
- **决定：** V1 不修改 Codex，也不接入 app-server；通过键盘、鼠标、macOS 应用激活和 Codex 现有快捷键工作。
- **原因：** 当前目标是验证手持控制体验，而不是同时承担实验性协议集成风险。
- **影响：** 轨道 A 无法可靠理解 Codex 的运行、等待输入和等待审批状态。
- **证据或验证：** 构建依赖中不得新增 Codex SDK、JSON-RPC 或 app-server schema。

### D-005 使用与许可范围

- **状态：** Accepted
- **决定：** 当前项目仅作为个人本机工具，并保留上游 GPL-3.0-or-later、NOTICE 和来源历史。
- **原因：** 私人使用可以直接沿用上游；分发或闭源商业化需要重新评估许可和私有框架依赖。
- **影响：** V1 不设计 App Store、沙盒、公证、商业授权或安装器发布流程。
- **证据或验证：** 根目录 [LICENSE](LICENSE) 和 [NOTICE](NOTICE) 必须保留，Git 历史必须能追溯到上游固定提交。

### D-006 触控圆盘

- **状态：** Accepted
- **决定：** 触控圆盘负责鼠标移动、轻触点击和现有圆周滚动行为。
- **原因：** Codex 中中心键会改为 Return，因此鼠标点击需要由触控轻触承担。
- **影响：** 触控算法沿用上游，不在 V1 重写手势识别。
- **证据或验证：** 现有圆周滚动/移动路径见 [TouchHandler.swift](app/TouchHandler.swift#L510-L547)，轻触点击路径见 [TouchHandler.swift](app/TouchHandler.swift#L656-L660)；实机阶段验证移动、轻触和滚动。

### D-007 中心键

- **状态：** Accepted
- **决定：** Codex 前台时，中心键完整单击发送 Enter；Chrome 和其他应用保留上游鼠标按下、拖拽和释放行为。
- **原因：** Codex 中 Enter 同时覆盖提交和当前审批选项确认；其他应用仍需要中心键鼠标拖拽。
- **影响：** Codex 中暂不提供中心键拖拽，改用触控轻触进行鼠标点击。
- **证据或验证：** Codex profile 的 workflow 覆盖与其他应用的鼠标回退共用同一分支，见 [RemoteInputHandler.swift](app/RemoteInputHandler.swift#L690-L700)。

### D-008 方向环

- **状态：** Accepted
- **决定：** 方向环发送 Up、Down、Left、Right。
- **原因：** 用于 Codex 菜单、审批选项和普通键盘导航。
- **影响：** 不为方向环增加 Codex 私有行为。
- **证据或验证：** 示例配置必须明确列出四个方向绑定，模拟输入测试验证键码。

### D-009 返回键

- **状态：** Accepted
- **决定：** 返回键发送单次 Escape，用于取消；审批界面中等价于拒绝当前审批。
- **原因：** 与 Codex 当前键盘交互一致。
- **影响：** 运行阶段单次 Escape 可能只进入停止确认，不保证直接中断。
- **证据或验证：** 模拟输入测试必须确认一次物理单击只产生一次 Escape。

### D-010 Play/Pause 中断

- **状态：** Accepted
- **决定：** Play/Pause 完整单击发送两次 Escape，两次之间固定间隔 200 ms。
- **原因：** 当前 Codex Desktop 的停止流程为第一次 Escape 进入停止确认、第二次 Escape 执行停止。
- **影响：** 按住、重复和释放事件不能再次触发中断；Codex 升级后必须重新验证该行为。
- **证据或验证：** 模拟事件 sink 断言 Escape、200 ms、Escape 的顺序和次数；实机阶段连续执行 10 次中断。

### D-011 审批阶段的 Play/Pause 风险

- **状态：** Accepted
- **决定：** Play/Pause 不做审批态保护。
- **原因：** 预期审批操作只使用方向环、中心键、返回键以及存在文字框时的听写，不使用 Play/Pause。
- **影响：** 审批阶段误按 Play/Pause 时，第一次 Escape 可能拒绝审批，第二次 Escape 还可能作用于审批结束后的界面；该误触风险已接受。
- **证据或验证：** A2854 实机阶段单独验证一次并把观察结果追加到变更记录或验证记录。

### D-012 Siri 键与豆包听写

- **状态：** Pending verification
- **决定：** Siri 键在所有应用中采用真实的 Fn-down/Fn-up；按住期间交给豆包输入法听写，松开时释放 Fn，使用 Mac 麦克风。
- **原因：** 用户决定统一使用豆包输入法，不调用 Codex 原生听写，也不使用遥控器麦克风。
- **影响：** 必须保证断连、退出、配置热重载和异常取消时释放 Fn；不得产生自动重复。
- **证据或验证：** macOS 提供 Fn 事件标志，但豆包是否接受合成 Fn 必须实机验证。Fn keycode/flag 见 [KeyMap.swift](app/KeyMap.swift#L69-L70)，真实按下/释放见 [MacActionExecutor.swift](app/MacActionExecutor.swift#L100-L109)。

### D-013 Codex 与 Chrome 切换

- **状态：** Accepted
- **决定：** TV 键在 Codex 和 Chrome 间切换；Codex 前台时激活 `com.google.Chrome`，其他情况下激活 `com.openai.codex`。
- **原因：** 形成 Codex 与浏览器之间的一键工作流。
- **影响：** Chrome 未安装时不得静默失败，应记录日志；V1 不实现应用轮盘替代。
- **证据或验证：** 前台应用 mock 覆盖 Codex、Chrome、其他应用三个分支。

### D-014 审批与文字输入操作

- **状态：** Accepted
- **决定：** 审批阶段用上/下选择、中心键确认、返回键拒绝；只有存在可编辑文字输入时，Siri/Fn 听写才有输入目标。
- **原因：** 二选一审批界面本身没有语音文本输入位置。
- **影响：** 不把语音输入误写成所有审批界面都必然可用。
- **证据或验证：** 软件阶段验证键盘事件，实机阶段在审批和等待用户输入两类界面分别验证。

### D-015 Codex 可配置快捷键

- **状态：** Accepted
- **决定：** Codex 支持大量可配置快捷键；当前本机版本没有可配置的 stop、interrupt、cancel-turn 或 retry 命令，因此 Play/Pause 使用双 Escape。
- **原因：** 支持的命令可以继续通过 Codex 设置与普通 keystroke 映射扩展，但中断不能假定存在可绑定命令。
- **影响：** V1 不自动修改用户的 Codex 快捷键文件。
- **证据或验证：** 快捷键页使用 `⌘-/` 打开，见 [Codex Changelog](https://help.openai.com/en/articles/11428266-codex-changelog/)。本机 Codex Desktop 版本为 `26.721.41059`。

### D-016 Fn 快捷键冲突

- **状态：** Accepted
- **决定：** 不把 Codex 的 `globalDictationHold` 或其他全局听写命令绑定为 Fn。
- **原因：** 避免 Codex 原生听写与豆包输入法同时响应。
- **影响：** 以后修改 Codex 快捷键时必须先做冲突检查并更新本文件。
- **证据或验证：** 当前 [keybindings.json](/Users/chengyizhou/.codex/keybindings.json#L1) 仅包含 Plan Mode 的 `Shift+Tab` 自定义绑定。

### D-017 遥控器麦克风

- **状态：** Accepted
- **决定：** V1 不启用 SiriRemoteForge 的遥控器麦克风、PacketLogger、LaunchDaemon、虚拟声卡或 root 安装流程。
- **原因：** 豆包听写使用 Mac 麦克风，远程麦克风路径私有、复杂且不属于 V1。
- **影响：** 不运行任何 `--capture-mic`、`--activate-mic`、`--native-ptt` 或 `--direct-ptt` 路径。
- **证据或验证：** 相关额外 HID 接口只应在显式麦克风诊断参数下加入，见 [RemoteDetector.swift](app/RemoteDetector.swift#L69-L87)；完成前进行静态搜索。

### D-018 完成状态

- **状态：** Accepted
- **决定：** “软件验证完成”和“A2854 实机验证完成”分开报告。
- **原因：** 当前没有硬件，静态检查和模拟事件不能证明真实 HID、触控与豆包听写行为。
- **影响：** 本轮最多声明软件验证完成；只有实机验收全部通过后才可标记 V1 完成。
- **证据或验证：** 验收报告必须分别列出软件命令证据和实机操作证据。

### D-019 仓库策略

- **状态：** Assumed
- **决定：** 在当前目录建立保留上游 Git 历史的本地分叉，不创建远端仓库、不推送。
- **原因：** 便于吸收上游 A2854 修复、追踪 GPL 来源，并保留当前本地审计日志。
- **影响：** 当前开发分支为 `codex-remote-v1`；`logs/` 只做本地忽略。
- **证据或验证：** HEAD 必须指向或继承 `10581a960f0738e36516defb79db24b18df214d3`，现有日志哈希必须保持不变。

### D-020 轨道 B 接口

- **状态：** Accepted
- **决定：** 轨道 B 未来复用语义工作流意图接口，不在 V1 创建假状态机或空的 app-server 客户端。
- **原因：** 保留演进边界，同时避免为尚未实现的原生集成引入无效复杂度。
- **影响：** V1 只实现 macOS 执行器；未来审批必须绑定具体 request、thread 和 turn，不能仅凭“等待审批”盲目批准。
- **证据或验证：** 公共接口位于 [WorkflowIntentExecutor.swift](SiriRemoteCore/Sources/SiriRemoteCore/WorkflowIntentExecutor.swift#L12-L50)，独立于 CGEvent；当前构建不得包含 app-server 协议依赖。

## 环境与基线证据

- **上游固定提交：** `10581a960f0738e36516defb79db24b18df214d3`
- **本地开发分支：** `codex-remote-v1`
- **macOS：** `26.5.2`，Apple Silicon `arm64`
- **Swift：** `6.3.3`
- **Codex Desktop：** `26.721.41059`，bundle ID `com.openai.codex`
- **Chrome bundle ID：** `com.google.Chrome`
- **未修改基线构建：** `cd app && ./build.sh` 成功
- **2026-07-26 审计日志基线 SHA-256：** `4a45abf81d6dd4aa8e8a9aeca72f4e2e4890c477247f8c3f00c424e40be0b6bc`
- **2026-07-28 当前审计文件 SHA-256：** `cede00c793f77e7b38621760e495c81d652b2539eba9e39f419ffe9224039008`；文件修改时间为 2026-07-27 22:14:09，说明本地运行时在基线后更新过该文件。该文件仍由 `.git/info/exclude` 本地忽略，不纳入本次提交。
- **Codex 快捷键文件 SHA-256：** `674574ea650b8acaa02becef666fd300b3a34ea362e701e70139d5f9b890eab6`

## 软件验证状态

截至 2026-07-26，当前工作树达到“软件验证完成”，不等于 V1 整体完成：

- `cd app && ./build.sh`：修改后完整应用构建成功，目标为 `arm64-apple-macosx13.0`。
- [`tests/run-software-verification.sh`](tests/run-software-verification.sh#L1)：通过。覆盖 workflow JSON 序列化/热重载、Codex/Chrome/default profile 解析、方向环、Codex 中心 workflow 与 Chrome 未绑定中心键、Siri Fn 配对/引用计数/Router teardown、执行器 pass-through chain、Play/Pause 重复抑制与 200 ms 双 Escape、Back、TV 路由、Fn keycode/flag 和圆周滚动核心回归。
- `git diff --check`：通过。
- 上游固定提交仍为当前 `HEAD`，本轮实现尚未提交或推送；`upstream` 历史保持可追溯。
- `logs/` 仍由本地 exclude 忽略且不纳入提交；其运行时审计文件已在 2026-07-26 基线后发生更新，当前快照见环境证据，因此不再宣称日志内容与基线相同。`~/.codex/keybindings.json` 哈希保持为上面的记录值，本轮未修改。
- 默认 V1 配置不绑定 Power、音量或静音；未启动 `BuiltinMicFeeder`，没有新增 app-server、审批状态机、LaunchDaemon 或 root 安装调用。
- 上游 `swift test` 在当前 Command Line Tools 环境无法导入 `XCTest`，因此未把该命令伪报为通过；本轮新增逻辑由独立 Swift 可执行测试覆盖。安装完整 Xcode 后仍应补跑上游 XCTest 套件。
- `RemoteInputHandler` 的中心键分支、热重载释放、断连和退出接线已做静态调用链审计，见 [RemoteInputHandler.swift](app/RemoteInputHandler.swift#L690-L755) 和 [RemoteInputHandler.swift](app/RemoteInputHandler.swift#L1580-L1620)；当前 harness 没有构造真实 IOHID callback 或启动应用生命周期，不能把 Router 测试写成 handler/HID 端到端测试。
- A2854 已经购买并确认型号；实体按键去重、蓝牙断连以及豆包对合成 Fn 的响应仍有实机待验证项。特别是当前上游把 HID `value == 1` 视为按下；若 A2854 长按重复使用其它非零值，必须在实机日志中确认不会被误判为释放。

## A2854 实机验证状态

截至 2026-07-28，已完成以下实机验证：

- macOS 蓝牙系统数据确认 Apple Vendor ID `0x004C`、Product ID `0x0315`、固件 `0x0021` 和 BLE 连接。
- 用户在当前 Codex Remote V1 配置下确认触控圆盘可以移动鼠标。
- 用户确认方向环可以在菜单中上下移动。
- 用户确认 Codex 中中心键可以提交或确认。

以下项目尚未在本记录中取得实机通过证据，因此 V1 仍为“部分实机验证”，不能标记为“A2854 实机验证完成”：

- 触控轻触点击、圆周滚动和 Chrome 中心键拖拽。
- Siri 按住/松开触发豆包听写以及 Fn 不粘键。
- Play/Pause 双 Escape 中断、长按不重复和审批界面误按结果。
- Back 拒绝、TV 键切换、全部实体按钮单次触发。
- 五次断连/重连，以及 Siri 按住期间断连后强制释放 Fn。

## 变更记录

| 日期 | 决策 ID | 原决定 | 新决定 | 原因 | 影响 | 修改人 |
|---|---|---|---|---|---|---|
| 2026-07-26 | Initial | 无 | 建立 D-001 至 D-020 | 记录首次完整对齐结果 | 成为后续实现与审核真相源 | Codex / 用户对齐 |
| 2026-07-28 | D-001 | 目标硬件尚未购买，型号待实机验证 | 已购买并连接；系统确认为 Apple `0x004C` / `0x0315` | A2854 到货并完成蓝牙识别 | 进入分阶段实机验收 | 用户 / Codex |
