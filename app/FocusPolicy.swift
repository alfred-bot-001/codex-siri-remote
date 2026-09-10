import Foundation
import CoreGraphics

/// Recorded only after this program posts a remote cursor move, never from global mouse polling.
struct RemoteFocusMovement {
    let generation: UInt64
    let point: CGPoint
    let time: TimeInterval
    let dragging: Bool
}

/// Pure dwell policy: a fresh remote move is a one-shot opportunity, not an idle focus timer.
struct FocusDwellGate {
    private struct Pending {
        let point: CGPoint
        let since: TimeInterval
        let frontmostPID: Int32?
    }
    private var lastGeneration: UInt64?
    private var pending: Pending?
    private let dwell: TimeInterval = 0.15
    private let radius: CGFloat = 4
    private let maxAge: TimeInterval = 0.75

    mutating func reset(movement: RemoteFocusMovement?) {
        lastGeneration = movement?.generation
        pending = nil
    }

    mutating func candidate(movement: RemoteFocusMovement?, cursor: CGPoint,
                            now: TimeInterval, frontmostPID: Int32?,
                            mouseButtonDown: Bool) -> CGPoint? {
        guard let movement = movement else { pending = nil; return nil }
        let fresh = movement.generation != lastGeneration
        lastGeneration = movement.generation
        let age = now - movement.time
        guard age >= 0, age <= maxAge, !movement.dragging, !mouseButtonDown,
              distance(cursor, movement.point) <= radius else {
            pending = nil
            return nil
        }
        // A keyboard/app switch while dwelling is a newer expression of user intent.
        if let request = pending, request.frontmostPID != frontmostPID {
            pending = nil
            return nil
        }
        if fresh {
            if pending == nil || distance(pending!.point, movement.point) > radius {
                pending = Pending(point: movement.point, since: now, frontmostPID: frontmostPID)
            }
        }
        guard let request = pending, now - request.since >= dwell else { return nil }
        pending = nil
        return cursor
    }

    private func distance(_ a: CGPoint, _ b: CGPoint) -> CGFloat {
        hypot(a.x - b.x, a.y - b.y)
    }
}

/// Front-to-back normal windows. Empty space and windows obscured by another app never count.
struct FocusWindow {
    let pid: Int32
    let bounds: CGRect
}

enum FocusCoverage {
    static func candidate(under point: CGPoint, windows: [FocusWindow], display: CGRect,
                          minimum: CGFloat = 0.9) -> Int32? {
        guard display.contains(point), let owner = windows.first(where: { $0.bounds.contains(point) })?.pid,
              visibleFraction(owner: owner, windows: windows, display: display) >= minimum
        else { return nil }
        return owner
    }

    static func visibleFraction(owner: Int32, windows: [FocusWindow], display: CGRect) -> CGFloat {
        guard !display.isEmpty, !display.isInfinite, !display.isNull else { return 0 }
        let clipped = windows.compactMap { window -> FocusWindow? in
            let rect = window.bounds.intersection(display)
            guard !rect.isEmpty, !rect.isNull else { return nil }
            return FocusWindow(pid: window.pid, bounds: rect)
        }
        // Split into cells at window edges. The first window covering a cell is its visible owner.
        // This computes a real union and respects occlusion, without counting gaps or overlaps twice.
        let xs = Array(Set(clipped.flatMap { [$0.bounds.minX, $0.bounds.maxX] })).sorted()
        guard xs.count >= 2 else { return 0 }
        var area: CGFloat = 0
        for i in 0..<(xs.count - 1) {
            let x = (xs[i] + xs[i + 1]) / 2
            let strip = clipped.filter { $0.bounds.minX <= x && x < $0.bounds.maxX }
            let ys = Array(Set(strip.flatMap { [$0.bounds.minY, $0.bounds.maxY] })).sorted()
            guard ys.count >= 2 else { continue }
            for j in 0..<(ys.count - 1) {
                let mid = CGPoint(x: x, y: (ys[j] + ys[j + 1]) / 2)
                if strip.first(where: { $0.bounds.contains(mid) })?.pid == owner {
                    area += (xs[i + 1] - xs[i]) * (ys[j + 1] - ys[j])
                }
            }
        }
        return area / (display.width * display.height)
    }
}
