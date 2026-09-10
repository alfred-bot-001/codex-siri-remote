// Optional remote-driven focus. Startup, idle mouse movement and app switches never arm it.
// Only a fresh move emitted by CursorController can start a single dwell opportunity.
// Candidates must already visibly cover at least 90% of their display.

import AppKit
import ApplicationServices

final class FocusFollowsCursor {

    /// Off by default — this changes which app receives input, which no one should discover by
    /// surprise. Enabled from config (`settings.focusFollowsCursor`).
    var enabled: Bool = false {
        didSet {
            guard enabled != oldValue else { return }
            enabled ? start() : stop()
        }
    }

    private var timer: Timer?
    private let ownPID = ProcessInfo.processInfo.processIdentifier

    private let movement: () -> RemoteFocusMovement?
    private var gate = FocusDwellGate()

    init(movement: @escaping () -> RemoteFocusMovement?) {
        self.movement = movement
    }

    deinit { stop() }

    // MARK: - Polling

    private func start() {
        stop()
        // 20 Hz: at a 0.15s dwell a 10 Hz poll leaves only one or two ticks inside the window, so
        // the delay you actually feel swings by a whole tick depending on where the cursor stopped.
        // The early-outs below mean a still cursor never reaches the expensive part either way.
        let t = Timer(timeInterval: 0.05, repeats: true) { [weak self] _ in self?.tick() }
        RunLoop.main.add(t, forMode: .common)
        timer = t
    }

    private func stop() {
        timer?.invalidate()
        timer = nil
        gate.reset(movement: movement())
    }

    private func tick() {
        guard enabled, let point = CGEvent(source: nil)?.location else { return }
        guard let candidate = gate.candidate(
            movement: movement(), cursor: point, now: ProcessInfo.processInfo.systemUptime,
            frontmostPID: NSWorkspace.shared.frontmostApplication?.processIdentifier,
            mouseButtonDown: NSEvent.pressedMouseButtons != 0
        ) else { return }
        focus(at: candidate)
    }

    // MARK: - Focusing

    private func focus(at point: CGPoint) {
        guard let pid = fillingAppPID(under: point), pid != ownPID else { return }
        guard NSWorkspace.shared.frontmostApplication?.processIdentifier != pid else { return }
        guard let app = NSRunningApplication(processIdentifier: pid),
              app.activationPolicy == .regular else { return }

        // Respect macOS if it declines cooperative activation; never force AXFrontmost.
        if app.activate(options: []) {
            rmDebug("🎯 focus follows remote cursor → \(app.localizedName ?? String(pid)) (activate)")
        }
    }

    /// PID of the app under `point`, but only if that app's windows already cover essentially the
    /// whole display the point is on. Nil otherwise — including when something small sits on top.
    private func fillingAppPID(under point: CGPoint) -> pid_t? {
        let options: CGWindowListOption = [.optionOnScreenOnly, .excludeDesktopElements]
        guard let windows = CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]],
              let display = displayBounds().first(where: { $0.contains(point) }),
              display.width > 0, display.height > 0
        else { return nil }

        let normal = windows.compactMap { window -> FocusWindow? in
            guard (window[kCGWindowLayer as String] as? Int) == 0,           // ordinary app windows
                  (window[kCGWindowAlpha as String] as? Double ?? 1) > 0.01,
                  let pid = window[kCGWindowOwnerPID as String] as? pid_t,
                  let dict = window[kCGWindowBounds as String] as? NSDictionary,
                  let bounds = CGRect(dictionaryRepresentation: dict)
            else { return nil }
            return FocusWindow(pid: pid, bounds: bounds)
        }

        return FocusCoverage.candidate(under: point, windows: normal, display: display)
    }

    private func displayBounds() -> [CGRect] {
        var ids = [CGDirectDisplayID](repeating: 0, count: 16)
        var count: UInt32 = 0
        guard CGGetActiveDisplayList(16, &ids, &count) == .success else { return [] }
        return ids.prefix(Int(count)).map { CGDisplayBounds($0) }
    }
}
