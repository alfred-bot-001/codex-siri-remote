import Foundation

@main
enum FocusPolicyVerification {
    static func main() {
        var checks = 0
        func expect(_ ok: @autoclosure () -> Bool, _ name: String) {
            precondition(ok(), name)
            checks += 1
        }
        let p = CGPoint(x: 500, y: 300)
        func move(_ generation: UInt64, _ time: TimeInterval, _ point: CGPoint? = nil,
                  dragging: Bool = false) -> RemoteFocusMovement {
            RemoteFocusMovement(generation: generation, point: point ?? p, time: time, dragging: dragging)
        }
        var gate = FocusDwellGate()
        func sample(_ movement: RemoteFocusMovement?, _ now: TimeInterval,
                    cursor: CGPoint? = nil, pid: Int32 = 7, down: Bool = false) -> CGPoint? {
            gate.candidate(movement: movement, cursor: cursor ?? p, now: now,
                           frontmostPID: pid, mouseButtonDown: down)
        }
        gate.reset(movement: nil)
        expect(sample(nil, 0) == nil, "Startup without remote input")
        expect(sample(nil, 100) == nil, "Stationary cursor never activates after startup")
        expect(sample(nil, 101, cursor: CGPoint(x: 800, y: 100)) == nil, "Ordinary mouse cannot arm focus")
        gate.reset(movement: move(1, 0))
        expect(sample(move(1, 0), 0.2) == nil, "Enabling ignores a pre-existing remote move")
        expect(sample(move(2, 1), 1) == nil, "Fresh remote move waits for dwell")
        expect(sample(move(2, 1), 1.1) == nil, "No early activation")
        expect(sample(move(2, 1), 1.2) == p, "Fresh remote move can focus after settling")
        expect(sample(move(2, 1), 1.3) == nil, "Each opportunity is consumed once")
        expect(sample(move(2, 1), 20) == nil, "Idle polling cannot refocus")

        gate.reset(movement: nil)
        _ = sample(move(3, 2), 2)
        expect(sample(move(3, 2), 2.1, pid: 8) == nil, "App switch cancels pending focus")
        expect(sample(move(3, 2), 2.3, pid: 8) == nil, "Cancelled app-switch request stays cancelled")
        gate.reset(movement: nil)
        _ = sample(move(4, 3), 3)
        expect(sample(move(4, 3), 3.1, cursor: CGPoint(x: 900, y: 900)) == nil, "Mouse movement cancels remote dwell")
        expect(sample(move(4, 3), 3.2) == nil, "Moving the mouse back cannot revive dwell")
        gate.reset(movement: nil)
        _ = sample(move(5, 4), 4)
        expect(sample(move(5, 4), 4.1, down: true) == nil, "Button press cancels focus")
        expect(sample(move(5, 4), 4.2) == nil, "Release does not revive cancelled focus")
        expect(sample(move(6, 5, dragging: true), 5) == nil, "Remote drag cannot arm focus")
        expect(sample(move(6, 5, dragging: true), 5.2) == nil, "Remote drag remains suppressed")
        expect(sample(move(7, 6), 8) == nil, "Stale move after wake or busy main thread is ignored")
        gate.reset(movement: nil)
        _ = sample(move(8, 9), 9)
        gate.reset(movement: move(8, 9))
        expect(sample(move(8, 9), 9.2) == nil, "Disable and re-enable drops pending focus")
        gate.reset(movement: nil)
        _ = sample(move(9, 10), 10)
        let next = CGPoint(x: 600, y: 300)
        expect(sample(move(10, 10.1, next), 10.1, cursor: next) == nil, "Continued remote motion restarts dwell")
        expect(sample(move(10, 10.1, next), 10.2, cursor: next) == nil, "Dwell is relative to last significant move")
        expect(sample(move(10, 10.1, next), 10.3, cursor: next) == next, "New resting point activates once")

        let screen = CGRect(x: 0, y: 0, width: 100, height: 100)
        func window(_ pid: Int32, _ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat) -> FocusWindow {
            FocusWindow(pid: pid, bounds: CGRect(x: x, y: y, width: w, height: h))
        }
        func fraction(_ windows: [FocusWindow]) -> CGFloat {
            FocusCoverage.visibleFraction(owner: 1, windows: windows, display: screen)
        }
        let edges = [window(1, 0, 0, 10, 100), window(1, 90, 0, 10, 100)]
        expect(abs(fraction(edges) - 0.2) < 0.00001, "Gaps between windows do not count as coverage")
        expect(FocusCoverage.candidate(under: CGPoint(x: 5, y: 50), windows: edges, display: screen) == nil,
               "Two distant narrow windows do not qualify as maximized")
        expect(abs(fraction([window(1, 0, 0, 60, 100), window(1, 40, 0, 40, 100)]) - 0.8) < 0.00001,
               "Overlapping owner windows are counted once")
        let split = [window(1, 0, 0, 100, 5), window(1, 0, 5, 100, 95)]
        expect(fraction(split) == 1, "Separate browser titlebar and content combine correctly")
        expect(FocusCoverage.candidate(under: CGPoint(x: 50, y: 50), windows: split, display: screen) == 1,
               "Visible full-display app remains eligible")
        expect(abs(fraction([window(2, 0, 0, 30, 100), window(1, 0, 0, 100, 100)]) - 0.7) < 0.00001,
               "Other apps covering the candidate reduce visible coverage")
        expect(fraction([window(1, 0, 0, 100, 100), window(2, 0, 0, 30, 100)]) == 1,
               "Windows behind the candidate do not reduce coverage")
        expect(fraction([window(1, -100, -100, 300, 300)]) == 1, "Off-screen bounds are clipped")
        expect(FocusCoverage.candidate(under: CGPoint(x: 5, y: 5),
               windows: [window(2, 0, 0, 10, 10), window(1, 0, 0, 100, 100)], display: screen) == nil,
               "Never reach through a small window under the pointer")
        expect(fraction([]) == 0, "Empty window list is harmless")
        expect(FocusCoverage.visibleFraction(owner: 1, windows: split, display: .zero) == 0,
               "Empty display does not divide by zero")
        print("PASS: \(checks) focus regression checks")
    }
}
