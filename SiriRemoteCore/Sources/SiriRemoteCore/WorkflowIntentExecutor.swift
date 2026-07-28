//
//  WorkflowIntentExecutor.swift
//  Codex Remote V1
//
//  Semantic workflow boundary. The executor knows the user-facing intent; the sink owns the
//  current macOS mechanism. A future Codex-native implementation can conform to the same protocol
//  without teaching the HID layer about app-server state.
//

import Foundation

public enum InputPhase: Equatable {
    case began
    case ended
    case tapped
}

public struct FrontmostAppContext: Equatable {
    public let bundleIdentifier: String?

    public init(bundleIdentifier: String?) {
        self.bundleIdentifier = bundleIdentifier
    }
}

public enum IntentExecutionResult: Equatable {
    case handled
    case passThrough
}

public protocol WorkflowIntentExecuting: AnyObject {
    @discardableResult
    func execute(
        _ intent: WorkflowIntent,
        phase: InputPhase,
        context: FrontmostAppContext
    ) -> IntentExecutionResult

    /// End any system-visible held input before disconnect, reload, or process teardown.
    func releaseHeldInputs()
}

/// Ordered fallback chain for Track B → Track A composition. An executor that cannot interpret an
/// intent/phase returns `passThrough`; the next executor then gets the same semantic input.
public final class WorkflowIntentExecutorChain: WorkflowIntentExecuting {
    private let executors: [WorkflowIntentExecuting]

    public init(_ executors: [WorkflowIntentExecuting]) {
        self.executors = executors
    }

    @discardableResult
    public func execute(
        _ intent: WorkflowIntent,
        phase: InputPhase,
        context: FrontmostAppContext
    ) -> IntentExecutionResult {
        for executor in executors {
            if executor.execute(intent, phase: phase, context: context) == .handled {
                return .handled
            }
        }
        return .passThrough
    }

    public func releaseHeldInputs() {
        for executor in executors {
            executor.releaseHeldInputs()
        }
    }
}

/// Injectable platform effects. Tests use a recording sink; production uses CGEvent and
/// NSWorkspace through `MacWorkflowEffectSink`.
public protocol WorkflowEffectSinking: AnyObject {
    func tapKey(_ keys: String)
    func beginFunctionHold() -> Bool
    func endFunctionHold()
    func schedule(after delay: TimeInterval, _ action: @escaping () -> Void)
    @discardableResult func activateApplication(bundleIdentifier: String) -> Bool
}

/// Converts raw physical button edges into stable workflow phases. Duplicate key-down reports are
/// ignored, and teardown ends holds without generating a tap.
public final class WorkflowInputRouter {
    private let executor: WorkflowIntentExecuting
    private var openIntents: [String: WorkflowIntent] = [:]

    public init(executor: WorkflowIntentExecuting) {
        self.executor = executor
    }

    public func isOpen(button: String) -> Bool {
        openIntents[button] != nil
    }

    public var openButtons: Set<String> {
        Set(openIntents.keys)
    }

    @discardableResult
    public func begin(
        button: String,
        intent: WorkflowIntent,
        context: FrontmostAppContext
    ) -> Bool {
        guard openIntents[button] == nil else { return false }
        guard executor.execute(intent, phase: .began, context: context) == .handled else {
            return false
        }
        openIntents[button] = intent
        return true
    }

    @discardableResult
    public func end(button: String, context: FrontmostAppContext) -> Bool {
        guard let intent = openIntents.removeValue(forKey: button) else { return false }
        _ = executor.execute(intent, phase: .ended, context: context)
        _ = executor.execute(intent, phase: .tapped, context: context)
        return true
    }

    @discardableResult
    public func cancel(button: String, context: FrontmostAppContext) -> Bool {
        guard let intent = openIntents.removeValue(forKey: button) else { return false }
        _ = executor.execute(intent, phase: .ended, context: context)
        return true
    }

    public func cancelAll(context: FrontmostAppContext) {
        for button in Array(openIntents.keys) {
            _ = cancel(button: button, context: context)
        }
        executor.releaseHeldInputs()
    }
}

public final class MacWorkflowIntentExecutor: WorkflowIntentExecuting {
    public static let codexBundleIdentifier = "com.openai.codex"
    public static let chromeBundleIdentifier = "com.google.Chrome"
    public static let interruptGap: TimeInterval = 0.2

    private let effects: WorkflowEffectSinking
    private var functionHoldCount = 0

    public init(effects: WorkflowEffectSinking) {
        self.effects = effects
    }

    @discardableResult
    public func execute(
        _ intent: WorkflowIntent,
        phase: InputPhase,
        context: FrontmostAppContext
    ) -> IntentExecutionResult {
        switch intent {
        case .primary:
            guard phase == .tapped else { return .handled }
            effects.tapKey("return")
            return .handled

        case .cancel:
            guard phase == .tapped else { return .handled }
            effects.tapKey("escape")
            return .handled

        case .interrupt:
            guard phase == .tapped else { return .handled }
            effects.tapKey("escape")
            effects.schedule(after: Self.interruptGap) { [weak effects] in
                effects?.tapKey("escape")
            }
            return .handled

        case .dictationHold:
            switch phase {
            case .began:
                if functionHoldCount == 0, !effects.beginFunctionHold() {
                    return .passThrough
                }
                functionHoldCount += 1
                return .handled
            case .ended:
                guard functionHoldCount > 0 else { return .handled }
                functionHoldCount -= 1
                if functionHoldCount == 0 {
                    effects.endFunctionHold()
                }
                return .handled
            case .tapped:
                return .handled
            }

        case .toggleCodexChrome:
            guard phase == .tapped else { return .handled }
            let target = context.bundleIdentifier == Self.codexBundleIdentifier
                ? Self.chromeBundleIdentifier
                : Self.codexBundleIdentifier
            _ = effects.activateApplication(bundleIdentifier: target)
            return .handled
        }
    }

    public func releaseHeldInputs() {
        guard functionHoldCount > 0 else { return }
        functionHoldCount = 0
        effects.endFunctionHold()
    }
}
