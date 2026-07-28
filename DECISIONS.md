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

- **状态：** Superseded
- **决定：** 当前项目仅作为个人本机工具，并保留上游 GPL-3.0-or-later、NOTICE 和来源历史。
- **原因：** 私人使用可以直接沿用上游；分发或闭源商业化需要重新评估许可和私有框架依赖。
- **影响：** V1 不设计 App Store、沙盒、公证、商业授权或安装器发布流程。
- **证据或验证：** 已由 D-029 替代；保留许可证、NOTICE 和上游历史的约束继续有效。

### D-006 触控圆盘

- **状态：** Accepted
- **决定：** 触控圆盘负责鼠标移动、轻触点击和现有圆周滚动行为。
- **原因：** 提供无需按下中心键的轻量鼠标点击，并保留上游成熟的触控交互。
- **影响：** 触控算法沿用上游，不在 V1 重写手势识别。
- **证据或验证：** 现有圆周滚动/移动路径见 [TouchHandler.swift](app/TouchHandler.swift#L510-L547)，轻触点击路径见 [TouchHandler.swift](app/TouchHandler.swift#L656-L660)；实机阶段验证移动、轻触和滚动。

### D-007 中心键

- **状态：** Superseded
- **决定：** Codex 前台时，中心键完整单击发送 Enter；Chrome 和其他应用保留上游鼠标按下、拖拽和释放行为。
- **原因：** Codex 中 Enter 同时覆盖提交和当前审批选项确认；其他应用仍需要中心键鼠标拖拽。
- **影响：** Codex 中暂不提供中心键拖拽，改用触控轻触进行鼠标点击。
- **证据或验证：** 已由 D-031 替代；`primary` 工作流意图作为未来接口保留，但当前配置不再把中心键绑定到它。

### D-008 方向环

- **状态：** Superseded
- **决定：** 方向环发送 Up、Down、Left、Right。
- **原因：** 用于 Codex 菜单、审批选项和普通键盘导航。
- **影响：** 不为方向环增加 Codex 私有行为。
- **证据或验证：** 已由 D-022 替代；Up/Down 和非 Codex 应用的 Left/Right 约束继续保留。

### D-009 返回键

- **状态：** Superseded
- **决定：** 返回键发送单次 Escape，用于取消；审批界面中等价于拒绝当前审批。
- **原因：** 与 Codex 当前键盘交互一致。
- **影响：** 运行阶段单次 Escape 可能只进入停止确认，不保证直接中断。
- **证据或验证：** 已由 D-024 替代；V1 不再提供返回键单 Escape。

### D-010 Play/Pause 中断

- **状态：** Superseded
- **决定：** Play/Pause 完整单击发送两次 Escape，两次之间固定间隔 200 ms。
- **原因：** 当前 Codex Desktop 的停止流程为第一次 Escape 进入停止确认、第二次 Escape 执行停止。
- **影响：** 按住、重复和释放事件不能再次触发中断；Codex 升级后必须重新验证该行为。
- **证据或验证：** 已由 D-026 替代；双 Escape 机制保留，但物理触发移到 TV 双击。

### D-011 审批阶段的 Play/Pause 风险

- **状态：** Superseded
- **决定：** Play/Pause 不做审批态保护。
- **原因：** 预期审批操作只使用方向环、中心键、返回键以及存在文字框时的听写，不使用 Play/Pause。
- **影响：** 审批阶段误按 Play/Pause 时，第一次 Escape 可能拒绝审批，第二次 Escape 还可能作用于审批结束后的界面；该误触风险已接受。
- **证据或验证：** 已由 D-026 和 D-027 替代；相同风险移到 TV 双击。

### D-012 Siri 键与豆包听写

- **状态：** Superseded
- **决定：** Siri 键在所有应用中采用真实的 Fn-down/Fn-up；按住期间交给豆包输入法听写，松开时释放 Fn，使用 Mac 麦克风。
- **原因：** 用户决定统一使用豆包输入法，不调用 Codex 原生听写，也不使用遥控器麦克风。
- **影响：** 必须保证断连、退出、配置热重载和异常取消时释放 Fn；不得产生自动重复。
- **证据或验证：** 已由 D-021 替代；Fn 配对和 teardown 约束仍然保留。

### D-013 Codex 与 Chrome 切换

- **状态：** Superseded
- **决定：** TV 键在 Codex 和 Chrome 间切换；Codex 前台时激活 `com.google.Chrome`，其他情况下激活 `com.openai.codex`。
- **原因：** 形成 Codex 与浏览器之间的一键工作流。
- **影响：** Chrome 未安装时不得静默失败，应记录日志；V1 不实现应用轮盘替代。
- **证据或验证：** 已由 D-023 和 D-024 替代；切换意图本身仍按三个前台应用分支执行。

### D-014 审批与文字输入操作

- **状态：** Superseded
- **决定：** 审批阶段用上/下选择、中心键确认、返回键拒绝；只有存在可编辑文字输入时，Siri/Fn 听写才有输入目标。
- **原因：** 二选一审批界面本身没有语音文本输入位置。
- **影响：** 不把语音输入误写成所有审批界面都必然可用。
- **证据或验证：** 已由 D-025 替代；文字输入边界继续保留。

### D-015 Codex 可配置快捷键

- **状态：** Superseded
- **决定：** Codex 支持大量可配置快捷键；当前本机版本没有可配置的 stop、interrupt、cancel-turn 或 retry 命令，因此 Play/Pause 使用双 Escape。
- **原因：** 支持的命令可以继续通过 Codex 设置与普通 keystroke 映射扩展，但中断不能假定存在可绑定命令。
- **影响：** V1 不自动修改用户的 Codex 快捷键文件。
- **证据或验证：** 中断命令边界仍然成立，但物理触发键已由 D-026 和 D-028 改为 TV 双击；快捷键页使用 `⌘-/` 打开，见 [Codex Changelog](https://help.openai.com/en/articles/11428266-codex-changelog/)。

### D-016 Fn 快捷键冲突

- **状态：** Accepted
- **决定：** 不把 Codex 的 `globalDictationHold` 或其他全局听写命令绑定为 Fn。
- **原因：** 避免 Codex 原生听写与豆包输入法同时响应。
- **影响：** 以后修改 Codex 快捷键时必须先做冲突检查并更新本文件。
- **证据或验证：** 发布前检查确认本轮未修改本机 `~/.codex/keybindings.json`；该用户文件不纳入仓库。

### D-017 遥控器麦克风

- **状态：** Accepted
- **决定：** V1 不启用 SiriRemoteForge 的遥控器麦克风、PacketLogger、LaunchDaemon、虚拟声卡或 root 安装流程。
- **原因：** 豆包听写使用 Mac 麦克风，远程麦克风路径私有、复杂且不属于 V1。
- **影响：** 不运行任何 `--capture-mic`、`--activate-mic`、`--native-ptt` 或 `--direct-ptt` 路径。
- **证据或验证：** 相关额外 HID 接口只应在显式麦克风诊断参数下加入，见 [RemoteDetector.swift](app/RemoteDetector.swift#L69-L87)；完成前进行静态搜索。

### D-018 完成状态

- **状态：** Superseded
- **决定：** “软件验证完成”和“A2854 实机验证完成”分开报告。
- **原因：** 当前没有硬件，静态检查和模拟事件不能证明真实 HID、触控与豆包听写行为。
- **影响：** 本轮最多声明软件验证完成；只有实机验收全部通过后才可标记 V1 完成。
- **证据或验证：** 已由 D-037 替代；软件证据和实机证据分开报告的原则继续有效。

### D-019 仓库策略

- **状态：** Superseded
- **决定：** 在当前目录建立保留上游 Git 历史的本地分叉，不创建远端仓库、不推送。
- **原因：** 便于吸收上游 A2854 修复、追踪 GPL 来源，并保留当前本地审计日志。
- **影响：** 当前开发分支为 `codex-remote-v1`；`logs/` 只做本地忽略。
- **证据或验证：** 已由 D-029 替代；当前分支仍须继承固定上游提交，`logs/` 继续只做本地忽略。

### D-020 轨道 B 接口

- **状态：** Accepted
- **决定：** 轨道 B 未来复用语义工作流意图接口，不在 V1 创建假状态机或空的 app-server 客户端。
- **原因：** 保留演进边界，同时避免为尚未实现的原生集成引入无效复杂度。
- **影响：** V1 只实现 macOS 执行器；未来审批必须绑定具体 request、thread 和 turn，不能仅凭“等待审批”盲目批准。
- **证据或验证：** 公共接口位于 [WorkflowIntentExecutor.swift](SiriRemoteCore/Sources/SiriRemoteCore/WorkflowIntentExecutor.swift#L12-L50)，独立于 CGEvent；当前构建不得包含 app-server 协议依赖。

### D-021 Siri 键先聚焦 Codex 输入框

- **状态：** Pending verification
- **决定：** Codex 前台时，Siri-down 先通过 macOS Accessibility 聚焦当前 Codex 窗口中位置最靠下的可用 `AXTextArea`，等待 120 ms 让 Electron 和输入法焦点稳定后再产生真实 Fn-down；Siri-up 仍产生 Fn-up。若在等待期间松开 Siri，则取消尚未发生的 Fn-down。其他应用保持直接 Fn-down/Fn-up。
- **原因：** 用户希望无论当前焦点位于聊天内容、菜单还是其他控件，按住 Siri 后都把豆包听写内容送入当前任务底部输入框。当前 Codex 没有公开、可配置的聚焦 composer 命令；Escape 只在空闲且无审批/浮层时可靠，不能作为通用聚焦键。
- **影响：** 该逻辑依赖 Codex 的 Accessibility 结构；聚焦失败时必须记录日志但继续原有 Fn 听写，不能吞掉 Siri 按键。Codex 升级后需重新验证输入框 role、位置和聚焦属性。
- **证据或验证：** 2026-07-28 当前 Codex 窗口探针找到底部 `AXTextArea`，描述为 `Do anything`，frame 为 `x=563 y=632 w=687 h=49`；设置 `AXFocused` 返回成功且目标节点回读 `AXFocused=1`。首次联合实测确认无延迟时第一次长按只完成聚焦、豆包未启动，后续长按可启动，因此增加 120 ms focus-settle 延迟。正式验收仍需复测第一次长按、松开释放和短按取消。

### D-022 Codex 方向环左右键

- **状态：** Accepted
- **决定：** Codex 前台时，方向环 Left 发送 `⌘-[`（返回导航历史），Right 发送 `⌘-]`（前进导航历史）；Up/Down 继续发送方向键。Chrome 和其他应用的 Left/Right 继续发送普通左右方向键。
- **原因：** 用户希望用实体圆环在最近访问过的 Codex session/页面之间返回和前进，同时保留审批菜单的上下选择能力。
- **影响：** Codex 中不再使用方向环 Left/Right 做文本光标水平移动；需要水平移动时使用触控鼠标或其他键位。该映射依赖 Codex 当前的 `navigateBack` / `navigateForward` 默认快捷键。
- **证据或验证：** Codex Desktop `26.721.41059` 的快捷键页面显示返回为 `⌘-[`、前进为 `⌘-]`；示例配置、当前用户配置和软件验证必须区分 Codex override 与 Chrome/global 继承行为。

### D-023 TV 键作为发送键

- **状态：** Superseded
- **决定：** TV/显示器键映射为 `primary`，完整单击发送 Return，作为独立的提交/发送键。该映射在 Codex、Chrome 和其他 profile 中继承生效。
- **原因：** 用户希望有一个位置明确、无需依赖中心触控点击语义的实体发送键。
- **影响：** TV 键不再负责 Codex/Chrome 切换；Chrome 或其他应用中按下 TV 也会发送 Return。
- **证据或验证：** 已由 D-026 替代；发送功能保留为 TV 单击。

### D-024 返回键作为应用切换键

- **状态：** Accepted
- **决定：** TV 键左侧的 `<` 返回键映射为 `toggleCodexChrome`：Codex 前台时切换到 Chrome，Chrome 或其他应用前台时切换到 Codex。
- **原因：** 用户把该实体位置指定为 Codex/Chrome 切换键。
- **影响：** 返回键不再发送单次 Escape，V1 不再有专用的取消/拒绝实体键；中断后来移到 TV 双击，Play/Pause 的当前行为以 D-028 为准。
- **证据或验证：** 配置解析必须确认 `button.menu → workflow.toggleCodexChrome`；实机分别从 Codex 和 Chrome 验证切换。

### D-025 新审批按键边界

- **状态：** Superseded
- **决定：** 审批阶段仍使用方向环 Up/Down 选择、中心键或 TV 键确认；当前布局没有专用的单 Escape 拒绝键。Siri/Fn 仅在存在可编辑文字输入时有输入目标。
- **原因：** D-024 将返回键改作应用切换，用户接受用实体布局优先保障发送与应用切换。
- **影响：** 需要拒绝或取消时使用触控鼠标点击界面；Play/Pause 双 Escape 不是安全的单次拒绝替代，仍有 D-011 所述误触风险。
- **证据或验证：** 已由 D-027 替代。

### D-026 TV 单击发送、双击中断

- **状态：** Superseded
- **决定：** TV/显示器键单击发送 Return；双击执行 `interrupt`，即 Escape、等待 200 ms、再 Escape。Play/Pause 移除 V1 绑定，恢复未绑定状态。
- **原因：** 用户希望把发送和中断集中到同一个实体键：单击发送、双击中断。
- **影响：** 为保证双击不会先触发单击发送，TV 的单击必须使用普通 keystroke 多击判定路径，不能使用会提前消费物理按下/释放的 `workflow.primary`。单击发送会等待 `doubleTapWindow`（当前默认约 300 ms）后执行；双击在第二次按下时立即执行且不发送 Return。
- **证据或验证：** 已由 D-028 替代；TV 单击/双击结论保留，Play/Pause 的未绑定结论被替换。

### D-027 TV 双击后的审批边界

- **状态：** Superseded
- **决定：** 审批阶段用方向环 Up/Down 选择、中心键或 TV 单击确认；当前没有专用单 Escape 拒绝键。TV 双击仍是无状态保护的双 Escape 中断，不作为安全的单次拒绝。
- **原因：** D-026 将原 Play/Pause 的双 Escape 行为移到 TV 双击，但没有改变轨道 A 不理解审批状态的边界。
- **影响：** 审批时误双击 TV 可能拒绝审批并让第二次 Escape 作用于后续界面；需要拒绝时优先使用触控鼠标点击。
- **证据或验证：** 已由 D-031 替代；TV 双击的无状态误触风险继续保留。

### D-028 Play/Pause 切换 Codex 边栏

- **状态：** Accepted
- **决定：** TV 键继续保持单击 Return、双击双 Escape 中断；Play/Pause 键仅在 Codex profile 中发送 `⌘-B`，用于切换 Codex 边栏。Chrome 和其他应用不绑定 Play/Pause，保留系统媒体播放/暂停行为。
- **原因：** 用户希望把 TV 键继续用于发送/中断，同时把其左下方 Play/Pause 实体键作为 Codex 边栏开关。
- **影响：** Codex 前台时 Play/Pause 不再控制媒体；离开 Codex 后恢复原生媒体行为。Codex 升级后必须重新验证 `⌘-B` 是否仍绑定 `toggleSidebar`。
- **证据或验证：** 配置解析必须分别确认 Codex 的 `button.playPause → keystroke(cmd+b)` 与 Chrome/global 的未绑定状态；实机在 Codex 中连续按两次验证边栏关闭和恢复。

### D-029 开源发布范围

- **状态：** Accepted
- **决定：** Codex Remote V1 作为 GPL-3.0-or-later 开源分叉整理，保留 SiriRemoteForge 的 Git 历史、LICENSE、NOTICE 和上游来源；不转为闭源，也不宣称适合 App Store、沙盒或公证分发。
- **原因：** 用户决定把已验证的手持 Codex 控制方案整理为开源项目，便于其他 A2854 用户复现和继续开发。
- **影响：** README 必须提供从购买型号、蓝牙配对、源码构建、权限授权、配置安装到验收的完整路径。当前仓库没有 `origin`，本次只创建可公开审核的本地提交；真正公开推送必须在用户指定 GitHub 账号和仓库后进行。
- **证据或验证：** 发布前检查提交继承上游 `10581a960f0738e36516defb79db24b18df214d3`，根目录许可证文件存在，提交中不含 `logs/`、用户配置、凭据或本机私有数据。

### D-030 README 部署说明

- **状态：** Accepted
- **决定：** 根目录 README 同时保留上游英文说明，并把 Codex Remote V1 中文快速开始置于上游通用说明之前。
- **原因：** 新用户需要先完成型号选择、配对和权限闭环，才能判断功能或代码是否有问题。
- **影响：** 中文说明须准确记录 A2854、返回键加音量加的配对方式、构建与 `.app` 生成、Accessibility 与 Input Monitoring、示例配置、当前键位、日志和软件 / 实机验收边界。
- **证据或验证：** 人工从空白环境逐条审阅命令和链接；构建、软件验证与 Markdown 差异检查通过后方可提交。

### D-031 中心键恢复鼠标点击

- **状态：** Accepted
- **决定：** 中心键在 Codex、Chrome 和其他应用中统一使用上游鼠标按下、点击、拖拽和释放行为；Codex profile 不再将 `button.select` 绑定到 `workflow.primary`。TV 单击成为唯一的实体 Return / 发送键，TV 双击继续中断。
- **原因：** 将鼠标操作与提交操作分配给不同实体键，避免中心键和 TV 键重复发送 Return。
- **影响：** Codex 审批时用方向环 Up/Down 选择并用 TV 单击确认；中心键只在鼠标指针实际位于目标按钮时通过点击确认。`primary` 语义意图仍保留给未来轨道 B，不删除公共接口。
- **证据或验证：** 示例配置和当前用户配置中 Codex mode 均不含 `button.select`；软件验证断言 Codex 与 Chrome 的中心键都未被配置引擎占用，从而回退到 [RemoteInputHandler.swift](app/RemoteInputHandler.swift#L690-L700) 的原生鼠标路径；实机验证 Codex 中单击和拖拽。

### D-032 降低圆周滚动灵敏度

- **状态：** Superseded
- **决定：** Codex Remote V1 的圆周滚动 `startThreshold` 从 `0.1` 提高到 `0.2` 弧度，`pixelsPerRadian` 从 `75` 降至 `50`；`scrollEase` 保持 `0.3`，速度增益参数继续使用上游默认值。
- **原因：** A2854 实机使用时，顺时针和逆时针滚动行为合理，但当前启动过早且滚动速度偏高。
- **影响：** 手指需旋转约 11.5° 才开始滚动；相同旋转角度的基础滚动距离降低约三分之一。快速旋转仍保留上游速度加速行为。
- **证据或验证：** 实机复测仍然偏快，已由 D-033 替代。

### D-033 再次降低圆周滚动速度和快速倍率

- **状态：** Superseded
- **决定：** `startThreshold` 保持 `0.2` 弧度，`pixelsPerRadian` 从 `50` 进一步降至 `40`；显式设置 `accelMin: 1.0` 和 `accelMax: 1.5`，取代上游默认的最高 `2.5×` 快速增益。`scrollEase` 继续保持 `0.3`。
- **原因：** D-032 实机复测后基础滚动仍偏快，快速转动时的默认增益也过高。
- **影响：** 基础滚动较 D-032 再降低 20%；快速转动的理论上限从每弧度 `125` 像素降到 `60` 像素。慢速旋转维持 `1.0×`，不额外放大或压低。
- **证据或验证：** 实机复测后仍希望稍慢，已由 D-034 替代。

### D-034 圆周滚动小幅再降速

- **状态：** Superseded
- **决定：** `pixelsPerRadian` 从 `40` 小幅降至 `35`；`startThreshold: 0.2`、`accelMin: 1.0`、`accelMax: 1.5` 和 `scrollEase: 0.3` 均保持不变。
- **原因：** D-033 的快速倍率已经合适，但整体滚动手感仍需稍慢。
- **影响：** 基础速度和各速度区间的最终滚动距离较 D-033 统一降低 12.5%，不会改变启动角度、平滑度或加速曲线形状。
- **证据或验证：** 实机复测后仍希望稍慢；设置界面已把当前快速倍率写为约 `1.304×`，已由 D-035 替代。

### D-035 按当前实测速率继续小幅降速

- **状态：** Accepted
- **决定：** 以热加载配置中实际读到的 `pixelsPerRadian: 35`、`accelMax: 1.303731343283582` 和 `startThreshold: 0.2` 为基线，只把基础速度降到 `30`；当前用户配置保留设置界面写入的约 `1.304×` 快速倍率，公开示例使用等价的舍入值 `1.3×`。
- **原因：** D-034 实机手感仍稍快；同时必须以当前配置文件为权威源，不能用之前记录的 `1.5×` 覆盖设置界面已写入的更低倍率。
- **影响：** 基础滚动距离较 D-034 再降低约 14.3%；快速倍率、启动角度和实际设置界面的其他调优值保持不变。
- **证据或验证：** 修改前直接读取 `~/.config/siriremote/config.jsonc` 确认当前值；修改后重新读取基础速度、等待热重载并运行软件验证。用户完成实机滚动复测后确认“好了”，接受当前手感。

### D-036 公开仓库目标

- **状态：** Accepted
- **决定：** 项目以公开 GitHub 仓库 `luobosibing2/codex-siri-remote` 发布，默认分支为 `main`；仓库标题使用 “Codex Siri Remote”，同时在 README 顶部和许可证说明中明确其基于 SiriRemoteForge。
- **原因：** 用户确认将当前实机调优版本作为开源项目提交，并选择 `codex-siri-remote` 作为公开仓库名。
- **影响：** README 的 clone 命令使用最终公开 URL；发布提交不得包含 `~/.config/siriremote/config.jsonc`、`logs/`、凭据或本机绝对路径。公开历史继续继承上游固定提交。
- **证据或验证：** 发布前运行敏感信息审计、构建、软件验证、`git diff --check` 和提交范围检查；创建公开仓库、推送 `main` 后再记录远端 URL 和提交。

### D-037 软件与实机完成边界

- **状态：** Accepted
- **决定：** 公开发布可以在软件验证通过且已明确标注实机覆盖范围时进行；“软件验证完成”“A2854 已连接并部分实测”和“A2854 全量验收完成”必须分开报告。
- **原因：** A2854 已购入并完成蓝牙识别、鼠标移动、方向导航和圆周滚动调优，但尚未取得所有键位、听写、断连和审批误触路径的逐项实机证据。
- **影响：** README 可以提供可复现的公开版本，但不得宣称全部硬件验收完成；未验证项目继续列在本文件中。
- **证据或验证：** 发布前软件命令必须重新执行；实机证据仅记录用户明确确认或日志直接观察到的项目。

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

截至 2026-07-29，发布候选工作树达到“软件验证完成”，不等于 A2854 全量验收完成：

- `cd app && ./build.sh`：修改后完整应用构建成功，目标为 `arm64-apple-macosx13.0`。
- [`tests/run-software-verification.sh`](tests/run-software-verification.sh#L1)：通过。覆盖 workflow JSON 序列化/热重载、Codex/Chrome/default profile 解析、方向环、Codex 与 Chrome 中心键均回退到原生鼠标路径、Siri Fn 配对/引用计数/Router teardown、执行器 pass-through chain、interrupt 重复抑制与 200 ms 双 Escape、Back、TV 路由、Fn keycode/flag 和圆周滚动核心回归。
- `git diff --check`：通过。
- Git ancestry 检查确认发布分支继承上游固定提交 `10581a960f0738e36516defb79db24b18df214d3`。
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
- 用户完成圆周滚动方向与多轮速度实测，接受基础速度 `30`、快速倍率约 `1.3×` 和启动阈值 `0.2` 的公开示例基线。
- 用户曾确认旧布局下 Codex 中心键可以提交或确认；该布局已由 D-031 替代，新的鼠标点击 / 拖拽行为待实机验证。
- Accessibility 探针确认当前 Codex 底部 composer 暴露为可聚焦的 `AXTextArea`，且目标节点聚焦回读成功；尚未完成 Siri 实键与豆包联合验证。

以下项目尚未在本记录中取得实机通过证据，因此 V1 仍为“部分实机验证”，不能标记为“A2854 实机验证完成”：

- 触控轻触点击，以及 Codex / Chrome 中心键点击与拖拽。
- Siri 按住/松开触发豆包听写以及 Fn 不粘键。
- TV 单击发送、TV 双击中断、审批界面误双击结果。
- Back 的 Codex / Chrome 切换、Play/Pause 的 Codex 边栏切换，以及全部实体按钮单次触发。
- 五次断连/重连，以及 Siri 按住期间断连后强制释放 Fn。

## 变更记录

| 日期 | 决策 ID | 原决定 | 新决定 | 原因 | 影响 | 修改人 |
|---|---|---|---|---|---|---|
| 2026-07-26 | Initial | 无 | 建立 D-001 至 D-020 | 记录首次完整对齐结果 | 成为后续实现与审核真相源 | Codex / 用户对齐 |
| 2026-07-28 | D-001 | 目标硬件尚未购买，型号待实机验证 | 已购买并连接；系统确认为 Apple `0x004C` / `0x0315` | A2854 到货并完成蓝牙识别 | 进入分阶段实机验收 | 用户 / Codex |
| 2026-07-28 | D-012 | 所有应用中 Siri 直接产生 Fn-down/Fn-up | 由 D-021 替代：Codex 先聚焦底部输入框，再产生 Fn-down；其他应用不变 | 确保豆包听写进入当前 Codex 任务输入框 | 新增 Codex Accessibility 聚焦步骤和升级回归要求 | 用户 / Codex |
| 2026-07-28 | D-021 | AX 聚焦后立即产生 Fn-down | 聚焦后等待 120 ms；等待期间松开则取消 Fn-down | 首次长按的 Fn 被焦点切换吞掉 | 修复首次长按并保留防粘键约束 | 用户 / Codex |
| 2026-07-28 | D-008 | 所有应用方向环发送 Up/Down/Left/Right | 由 D-022 替代：Codex Left/Right 改为导航历史返回/前进，其他应用不变 | 用实体圆环切换 Codex session/页面 | Codex 内失去左右光标键，新增快捷键升级回归 | 用户 / Codex |
| 2026-07-28 | D-009 / D-013 / D-014 | 返回键单 Escape；TV 切换 Codex/Chrome；返回键拒绝审批 | 由 D-023 至 D-025 替代：TV 发送，返回键切换应用，不再提供专用单 Escape | 用户重新指定两个实体键位 | 审批拒绝改由触控操作，TV 在其他应用也发送 Return | 用户 / Codex |
| 2026-07-28 | D-010 / D-011 / D-023 / D-025 | Play/Pause 单击双 Escape；TV 单击发送 | 由 D-026 和 D-027 替代：TV 单击发送、双击中断，Play/Pause 未绑定 | 用户把双击中断集中到 TV 键 | TV 单击增加多击判定延迟，审批误触风险转移到 TV 双击 | 用户 / Codex |
| 2026-07-28 | D-026 | TV 单击发送、双击中断；Play/Pause 未绑定 | 由 D-028 替代：TV 行为不变，Play/Pause 在 Codex 中发送 `⌘-B` | 用户指定 Play/Pause 切换 Codex 边栏 | Codex 内不再控制媒体，其他应用保持原生媒体行为 | 用户 / Codex |
| 2026-07-29 | D-005 / D-019 | 个人本机工具；仅保留本地分叉 | 由 D-029 替代：按 GPL 开源分叉整理，当前先创建可公开审核的本地提交 | 用户决定把项目开源 | 增加公开文档和发布审计，实际远端发布仍需指定目标仓库 | 用户 / Codex |
| 2026-07-29 | D-030 | 仅有简短英文 Codex profile | 增加置顶中文快速开始和完整部署路径 | 让新用户能从购买和配对开始复现 | README 成为公开安装入口，决策记录继续作为行为真相源 | 用户 / Codex |
| 2026-07-29 | D-007 / D-027 | Codex 中心键发送 Return；中心键或 TV 可确认审批 | 由 D-031 替代：中心键在所有应用中恢复鼠标点击 / 拖拽，TV 单击独占发送 / 确认 | 避免中心键与 TV 键功能重复 | 审批键盘确认使用 TV；中心键仅按鼠标位置点击 | 用户 / Codex |
| 2026-07-29 | D-032 | 圆周滚动启动阈值 `0.1`、速度 `75` | 启动阈值 `0.2`、速度 `50` | A2854 实机滚动灵敏度偏高 | 启动角度增大，基础滚动距离降低约三分之一 | 用户 / Codex |
| 2026-07-29 | D-032 / D-033 | 启动阈值 `0.2`、速度 `50`、最高默认 `2.5×` 快速增益 | 阈值保持 `0.2`、速度降为 `40`、最高快速增益降为 `1.5×` | 首轮降速后实机仍偏快 | 基础速度再降 20%，快速转动上限明显降低 | 用户 / Codex |
| 2026-07-29 | D-033 / D-034 | 基础速度 `40`、最高快速增益 `1.5×` | 基础速度 `35`、快速增益保持 `1.5×` | 实机手感还需稍慢 | 所有速度区间的滚动距离统一降低 12.5% | 用户 / Codex |
| 2026-07-29 | D-034 / D-035 | 记录值为基础速度 `35`、最高快速增益 `1.5×` | 按实际配置读取值，将基础速度降到 `30`，保留当前约 `1.304×` 快速增益 | 实机仍需稍慢，且设置界面已更新快速倍率 | 基础距离再降约 14.3%，不覆盖当前 UI 调优 | 用户 / Codex |
| 2026-07-29 | D-035 | 当前滚动参数待实机确认 | 用户确认当前滚动手感可接受 | 完成多轮 A2854 实机调速 | 当前速度参数成为公开示例基线 | 用户 / Codex |
| 2026-07-29 | D-029 / D-036 | 按 GPL 开源整理，远端目标待定 | 发布到 `luobosibing2/codex-siri-remote`，默认分支 `main` | 用户选择公开仓库名称 | README 固化 clone URL，进入实际发布验证与推送 | 用户 / Codex |
| 2026-07-29 | D-015 | Play/Pause 使用双 Escape | 中断保持双 Escape，但由 TV 双击触发；Play/Pause 使用 `⌘-B` | 后续键位调整已经稳定 | 修正旧决定状态，避免公开文档产生冲突 | 用户 / Codex |
| 2026-07-29 | D-018 / D-037 | 无实体硬件时区分软件与未来实机验收 | 已有 A2854 部分实测，继续区分软件、部分实机和全量实机验收 | 硬件已购入并完成部分验证 | 允许边界清楚的公开发布，不宣称全量硬件完成 | 用户 / Codex |
| 2026-07-29 | D-024 | 返回键切换应用，但影响说明仍称 Play/Pause 中断 | 返回键行为不变；影响说明按 D-028 修正为 TV 双击中断、Play/Pause 切边栏 | 清理公开决策记录中的派生描述 | 不改变当前映射 | Codex |
