import Foundation
import CoreGraphics
import Carbon.HIToolbox

private enum VerificationFailure: Error, CustomStringConvertible {
    case failed(String)

    var description: String {
        switch self {
        case .failed(let message): return message
        }
    }
}

private func expect(
    _ condition: @autoclosure () -> Bool,
    _ message: String
) throws {
    if !condition() { throw VerificationFailure.failed(message) }
}

private final class RecordingEffects: WorkflowEffectSinking {
    var events: [String] = []
    var scheduled: [(delay: TimeInterval, action: () -> Void)] = []
    var activatedBundles: [String] = []

    func tapKey(_ keys: String) {
        events.append("tap:\(keys)")
    }

    func focusBottomTextArea(bundleIdentifier: String) -> Bool {
        events.append("focus:\(bundleIdentifier)")
        return true
    }

    func beginFunctionHold() -> Bool {
        events.append("fn:down")
        return true
    }

    func endFunctionHold() {
        events.append("fn:up")
    }

    func schedule(after delay: TimeInterval, _ action: @escaping () -> Void) {
        scheduled.append((delay, action))
    }

    func activateApplication(bundleIdentifier: String) -> Bool {
        activatedBundles.append(bundleIdentifier)
        return true
    }
}

private final class RecordingActionExecutor: ActionExecutor {
    var actions: [Action] = []

    func execute(_ action: Action, payload: EventPayload?) {
        actions.append(action)
    }
}

private final class PassThroughWorkflowExecutor: WorkflowIntentExecuting {
    var calls = 0

    func execute(
        _ intent: WorkflowIntent,
        phase: InputPhase,
        context: FrontmostAppContext
    ) -> IntentExecutionResult {
        calls += 1
        return .passThrough
    }

    func releaseHeldInputs() {}
}

@main
private struct SoftwareVerificationMain {
    static func main() throws {
        guard CommandLine.arguments.count == 2 else {
            throw VerificationFailure.failed("expected path to examples/codex-remote-v1.jsonc")
        }

        try verifyConfigAndSerialization(at: CommandLine.arguments[1])
        try verifyFunctionKeyMapping()
        try verifyPhysicalPhasesAndWorkflowEffects()

        print("PASS: Codex Remote V1 software verification")
    }

    private static func verifyConfigAndSerialization(at path: String) throws {
        let text = try String(contentsOfFile: path, encoding: .utf8)
        let config = try ConfigLoader.load(text)
        try expect(config.appProfiles["com.openai.codex"] == "codex", "Codex profile missing")
        try expect(config.appProfiles["com.google.Chrome"] == "chrome", "Chrome profile missing")
        try expect(config.appProfiles["default"] == "global", "default profile missing")
        try expect(config.modes["global"]?.bindings["button.power"] == nil, "Power must be unbound")
        try expect(config.modes["global"]?.bindings["button.volumeUp"] == nil, "volume must be native")
        try expect(
            config.settings.circularScroll.pixelsPerRadian == 30,
            "Codex profile circular-scroll base speed must remain reduced"
        )
        try expect(
            config.settings.circularScroll.accelMax == 1.3,
            "Codex profile circular-scroll fast gain must remain capped"
        )

        let actionExecutor = RecordingActionExecutor()
        let controller = Controller(engine: MappingEngine(config: config), executor: actionExecutor)
        controller.frontmostAppChanged(bundleID: "com.openai.codex")
        try expect(controller.currentMode == "codex", "Codex profile must resolve to codex mode")
        try expect(
            controller.resolvedAction(for: "button.select") == nil,
            "Codex center must fall through to upstream mouse handling"
        )
        try expect(
            controller.resolvedAction(for: "button.tv") == .keystroke(keys: "return"),
            "TV single tap must resolve to Return/send"
        )
        try expect(
            controller.resolvedAction(for: "button.tv.double")
                == .workflow(intent: .interrupt),
            "TV double tap must resolve to interrupt"
        )
        try expect(
            controller.resolvedAction(for: "button.menu")
                == .workflow(intent: .toggleCodexChrome),
            "Back button must resolve to Codex/Chrome toggle"
        )
        try expect(
            controller.resolvedAction(for: "button.playPause") == .keystroke(keys: "cmd+b"),
            "Codex Play/Pause must resolve to the sidebar shortcut"
        )
        for (key, expected) in [
            ("ring.up", "up"),
            ("ring.down", "down"),
            ("ring.left", "cmd+["),
            ("ring.right", "cmd+]"),
        ] {
            try expect(controller.handle(InputEvent(key: key)), "\(key) must be bound")
            try expect(
                actionExecutor.actions.last == .keystroke(keys: expected),
                "\(key) must resolve to \(expected)"
            )
        }
        controller.frontmostAppChanged(bundleID: "com.google.Chrome")
        try expect(controller.currentMode == "chrome", "Chrome profile must resolve to chrome mode")
        try expect(
            controller.resolvedAction(for: "button.select") == nil,
            "Chrome center must fall through to upstream mouse handling"
        )
        try expect(
            controller.resolvedAction(for: "button.tv") == .keystroke(keys: "return"),
            "Chrome must retain the TV send/Return binding"
        )
        try expect(
            controller.resolvedAction(for: "button.tv.double")
                == .workflow(intent: .interrupt),
            "Chrome must retain the TV double-interrupt binding"
        )
        try expect(
            controller.resolvedAction(for: "button.menu")
                == .workflow(intent: .toggleCodexChrome),
            "Chrome Back button must switch to Codex"
        )
        try expect(
            controller.resolvedAction(for: "ring.left") == .keystroke(keys: "left"),
            "Chrome ring-left must retain the global arrow binding"
        )
        try expect(
            controller.resolvedAction(for: "ring.right") == .keystroke(keys: "right"),
            "Chrome ring-right must retain the global arrow binding"
        )
        try expect(
            controller.resolvedAction(for: "button.playPause") == nil,
            "Chrome Play/Pause must remain unbound for native media behavior"
        )

        let serialized = try ConfigWriter.serialize(config)
        let reloaded = try ConfigLoader.load(serialized)
        try expect(reloaded == config, "workflow config must survive serialization and hot reload")

        let action = Action.workflow(intent: .toggleCodexChrome)
        let data = try JSONEncoder().encode(action)
        let decoded = try JSONDecoder().decode(Action.self, from: data)
        try expect(decoded == action, "workflow action JSON round-trip failed")

        let scroll = CircularScrollDetector(config: CircularScrollConfig(
            enabled: true,
            minRadius: 0.2,
            startThreshold: 0.05,
            pixelsPerRadian: 75,
            scrollEase: 0.3,
            invert: false
        ))
        _ = scroll.feed(x: 1.0, y: 0.5)
        let rotation = scroll.feed(x: 0.98, y: 0.65)
        try expect(rotation > 0, "existing circular-scroll detector must still emit rotation")
    }

    private static func verifyFunctionKeyMapping() throws {
        guard let combo = KeyMap.parse("fn") else {
            throw VerificationFailure.failed("fn did not parse")
        }
        try expect(combo.mainKey == nil, "Fn must be a modifier-only combo")
        try expect(combo.mods.count == 1, "Fn must contain exactly one modifier")
        try expect(combo.mods[0].keyCode == CGKeyCode(kVK_Function), "Fn key code must be 63")
        try expect(combo.mods[0].flag == .maskSecondaryFn, "Fn must carry maskSecondaryFn")
    }

    private static func verifyPhysicalPhasesAndWorkflowEffects() throws {
        let effects = RecordingEffects()
        let passThrough = PassThroughWorkflowExecutor()
        let macExecutor = MacWorkflowIntentExecutor(effects: effects)
        let executor = WorkflowIntentExecutorChain([passThrough, macExecutor])
        let router = WorkflowInputRouter(executor: executor)
        let other = FrontmostAppContext(bundleIdentifier: "com.example.Other")
        let codex = FrontmostAppContext(
            bundleIdentifier: MacWorkflowIntentExecutor.codexBundleIdentifier
        )

        try expect(
            router.begin(button: "siri", intent: .dictationHold, context: other),
            "Siri-down was not accepted"
        )
        try expect(
            !router.begin(button: "siri", intent: .dictationHold, context: other),
            "repeated Siri-down must be ignored"
        )
        try expect(router.end(button: "siri", context: other), "Siri-up was not accepted")
        try expect(effects.events == ["fn:down", "fn:up"], "Fn-down/up must be strictly paired")
        try expect(passThrough.calls > 0, "passThrough must reach the fallback executor")

        _ = router.begin(button: "siri", intent: .dictationHold, context: codex)
        try expect(
            effects.events.last
                == "focus:\(MacWorkflowIntentExecutor.codexBundleIdentifier)",
            "Codex Siri-down must focus the bottom text area first"
        )
        try expect(effects.scheduled.count == 1, "Codex Fn-down must wait for focus to settle")
        try expect(
            abs(effects.scheduled[0].delay - MacWorkflowIntentExecutor.composerFocusSettleDelay)
                < 0.000_001,
            "Codex focus-settle delay changed unexpectedly"
        )
        try expect(
            effects.events.last != "fn:down",
            "Codex Fn-down must not share the AX focus event cycle"
        )
        effects.scheduled[0].action()
        try expect(effects.events.last == "fn:down", "settled Codex focus must start Fn")
        _ = router.end(button: "siri", context: codex)
        try expect(effects.events.last == "fn:up", "Codex Siri-up must release delayed Fn")

        _ = router.begin(button: "siri", intent: .dictationHold, context: codex)
        _ = router.end(button: "siri", context: codex)
        let fnDownCountBeforeCancelledDelay = effects.events.filter { $0 == "fn:down" }.count
        effects.scheduled[1].action()
        try expect(
            effects.events.filter { $0 == "fn:down" }.count == fnDownCountBeforeCancelledDelay,
            "releasing Siri before focus settles must cancel the pending Fn-down"
        )
        effects.scheduled.removeAll()

        _ = router.begin(button: "siri", intent: .dictationHold, context: other)
        _ = router.begin(button: "alternateDictation", intent: .dictationHold, context: other)
        _ = router.end(button: "siri", context: other)
        try expect(effects.events.last == "fn:down", "first holder must not release shared Fn")
        _ = router.end(button: "alternateDictation", context: other)
        try expect(
            effects.events.suffix(2) == ["fn:down", "fn:up"],
            "last dictation holder must release Fn"
        )

        _ = router.begin(button: "siri", intent: .dictationHold, context: other)
        router.cancelAll(context: other)
        try expect(
            effects.events.suffix(2) == ["fn:down", "fn:up"],
            "teardown must force Fn-up"
        )

        _ = router.begin(button: "playPause", intent: .interrupt, context: other)
        try expect(
            !router.begin(button: "playPause", intent: .interrupt, context: other),
            "held/repeated Play/Pause must not open a second interrupt"
        )
        _ = router.end(button: "playPause", context: other)
        try expect(effects.events.last == "tap:escape", "interrupt must send first Escape")
        try expect(effects.scheduled.count == 1, "interrupt must schedule one second Escape")
        try expect(
            abs(effects.scheduled[0].delay - 0.2) < 0.000_001,
            "interrupt gap must be 200 ms"
        )
        effects.scheduled[0].action()
        try expect(
            effects.events.suffix(2) == ["tap:escape", "tap:escape"],
            "one Play/Pause tap must produce exactly two Escapes"
        )

        _ = router.begin(button: "select", intent: .primary, context: other)
        _ = router.end(button: "select", context: other)
        try expect(effects.events.last == "tap:return", "primary workflow intent must send Return")

        _ = router.begin(button: "menu", intent: .cancel, context: other)
        _ = router.end(button: "menu", context: other)
        try expect(effects.events.last == "tap:escape", "Back must send one Escape")

        _ = router.begin(button: "tv", intent: .toggleCodexChrome, context: codex)
        _ = router.end(button: "tv", context: codex)
        _ = router.begin(button: "tv", intent: .toggleCodexChrome, context: other)
        _ = router.end(button: "tv", context: other)
        try expect(
            effects.activatedBundles == [
                MacWorkflowIntentExecutor.chromeBundleIdentifier,
                MacWorkflowIntentExecutor.codexBundleIdentifier,
            ],
            "TV routing must toggle Codex/Chrome and send other apps to Codex"
        )
    }
}
