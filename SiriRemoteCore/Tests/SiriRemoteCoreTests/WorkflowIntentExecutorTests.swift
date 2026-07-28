import XCTest
@testable import SiriRemoteCore

final class WorkflowIntentExecutorTests: XCTestCase {
    private final class RecordingEffects: WorkflowEffectSinking {
        var events: [String] = []
        var scheduled: [(delay: TimeInterval, action: () -> Void)] = []
        var activatedBundles: [String] = []
        var canBeginFunctionHold = true

        func tapKey(_ keys: String) {
            events.append("tap:\(keys)")
        }

        func beginFunctionHold() -> Bool {
            events.append("fn:down")
            return canBeginFunctionHold
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

    private let other = FrontmostAppContext(bundleIdentifier: "com.example.Other")

    func testInterruptOnlyFiresOnTapAndSchedulesSecondEscapeAt200Milliseconds() {
        let effects = RecordingEffects()
        let executor = MacWorkflowIntentExecutor(effects: effects)

        XCTAssertEqual(executor.execute(.interrupt, phase: .began, context: other), .handled)
        XCTAssertEqual(executor.execute(.interrupt, phase: .ended, context: other), .handled)
        XCTAssertEqual(executor.execute(.interrupt, phase: .tapped, context: other), .handled)
        XCTAssertEqual(effects.events, ["tap:escape"])
        XCTAssertEqual(effects.scheduled.count, 1)
        XCTAssertEqual(effects.scheduled[0].delay, 0.2, accuracy: 0.000_001)

        effects.scheduled[0].action()
        XCTAssertEqual(effects.events, ["tap:escape", "tap:escape"])
    }

    func testFunctionHoldIsReferenceCounted() {
        let effects = RecordingEffects()
        let executor = MacWorkflowIntentExecutor(effects: effects)

        XCTAssertEqual(executor.execute(.dictationHold, phase: .began, context: other), .handled)
        XCTAssertEqual(executor.execute(.dictationHold, phase: .began, context: other), .handled)
        XCTAssertEqual(effects.events, ["fn:down"])

        XCTAssertEqual(executor.execute(.dictationHold, phase: .ended, context: other), .handled)
        XCTAssertEqual(executor.execute(.dictationHold, phase: .ended, context: other), .handled)
        XCTAssertEqual(effects.events, ["fn:down", "fn:up"])
    }

    func testExecutorChainFallsThroughToNextExecutor() {
        final class PassThroughExecutor: WorkflowIntentExecuting {
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

        let first = PassThroughExecutor()
        let effects = RecordingEffects()
        let fallback = MacWorkflowIntentExecutor(effects: effects)
        let chain = WorkflowIntentExecutorChain([first, fallback])

        XCTAssertEqual(chain.execute(.cancel, phase: .tapped, context: other), .handled)
        XCTAssertEqual(first.calls, 1)
        XCTAssertEqual(effects.events, ["tap:escape"])
    }

    func testForcedReleaseEndsFunctionHold() {
        let effects = RecordingEffects()
        let executor = MacWorkflowIntentExecutor(effects: effects)

        _ = executor.execute(.dictationHold, phase: .began, context: other)
        executor.releaseHeldInputs()
        executor.releaseHeldInputs()

        XCTAssertEqual(effects.events, ["fn:down", "fn:up"])
    }

    func testPrimaryAndCancelAreTapOnly() {
        let effects = RecordingEffects()
        let executor = MacWorkflowIntentExecutor(effects: effects)

        _ = executor.execute(.primary, phase: .began, context: other)
        _ = executor.execute(.primary, phase: .tapped, context: other)
        _ = executor.execute(.cancel, phase: .tapped, context: other)

        XCTAssertEqual(effects.events, ["tap:return", "tap:escape"])
    }

    func testToggleRoutesFromCodexToChromeAndOtherwiseToCodex() {
        let effects = RecordingEffects()
        let executor = MacWorkflowIntentExecutor(effects: effects)

        _ = executor.execute(
            .toggleCodexChrome,
            phase: .tapped,
            context: FrontmostAppContext(
                bundleIdentifier: MacWorkflowIntentExecutor.codexBundleIdentifier
            )
        )
        _ = executor.execute(
            .toggleCodexChrome,
            phase: .tapped,
            context: FrontmostAppContext(
                bundleIdentifier: MacWorkflowIntentExecutor.chromeBundleIdentifier
            )
        )
        _ = executor.execute(.toggleCodexChrome, phase: .tapped, context: other)

        XCTAssertEqual(effects.activatedBundles, [
            MacWorkflowIntentExecutor.chromeBundleIdentifier,
            MacWorkflowIntentExecutor.codexBundleIdentifier,
            MacWorkflowIntentExecutor.codexBundleIdentifier,
        ])
    }

    func testPhysicalRouterIgnoresRepeatAndTeardownDoesNotTap() {
        let effects = RecordingEffects()
        let executor = MacWorkflowIntentExecutor(effects: effects)
        let router = WorkflowInputRouter(executor: executor)

        XCTAssertTrue(router.begin(button: "siri", intent: .dictationHold, context: other))
        XCTAssertFalse(router.begin(button: "siri", intent: .dictationHold, context: other))
        router.cancelAll(context: other)
        XCTAssertEqual(effects.events, ["fn:down", "fn:up"])

        XCTAssertTrue(router.begin(button: "playPause", intent: .interrupt, context: other))
        XCTAssertFalse(router.begin(button: "playPause", intent: .interrupt, context: other))
        XCTAssertTrue(router.end(button: "playPause", context: other))
        XCTAssertFalse(router.end(button: "playPause", context: other))
        XCTAssertEqual(effects.events, ["fn:down", "fn:up", "tap:escape"])
        XCTAssertEqual(effects.scheduled.count, 1)
    }
}
