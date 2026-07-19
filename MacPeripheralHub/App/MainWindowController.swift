import AppKit

final class MainWindowController: NSWindowController, NSWindowDelegate {
    private static let frameAutosaveName = "MacPeripheralHubMainWindow"
    private let onWindowHidden: () -> Void

    init(appState: AppState, onWindowHidden: @escaping () -> Void) {
        self.onWindowHidden = onWindowHidden
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1080, height: 700),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "MacPeripheralHub"
        window.minSize = NSSize(width: 860, height: 560)
        window.tabbingMode = .disallowed
        window.isReleasedWhenClosed = false
        window.collectionBehavior = [.managed, .fullScreenPrimary]
        window.setFrameAutosaveName(Self.frameAutosaveName)
        if !window.setFrameUsingName(Self.frameAutosaveName) {
            window.center()
        }
        Self.ensureWindowFrameIsVisible(window)
        window.contentView = RootView(appState: appState)

        super.init(window: window)
        window.delegate = self
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("MainWindowController does not support storyboard initialization.")
    }

    func windowDidMove(_ notification: Notification) {
        window?.saveFrame(usingName: Self.frameAutosaveName)
    }

    func windowDidResize(_ notification: Notification) {
        window?.saveFrame(usingName: Self.frameAutosaveName)
    }

    func windowShouldClose(_ sender: NSWindow) -> Bool {
        sender.saveFrame(usingName: Self.frameAutosaveName)
        sender.orderOut(nil)
        onWindowHidden()
        return false
    }

    func showAndActivate(_ sender: Any?) {
        guard let window else {
            return
        }

        Self.ensureWindowFrameIsVisible(window)
        showWindow(sender)
        window.deminiaturize(sender)
        window.makeKeyAndOrderFront(sender)
        window.orderFrontRegardless()
    }

    private static func ensureWindowFrameIsVisible(_ window: NSWindow) {
        let visibleFrames = NSScreen.screens.map(\.visibleFrame)
        guard !visibleFrames.contains(where: { $0.intersects(window.frame) }) else {
            return
        }

        if let targetFrame = NSScreen.main?.visibleFrame ?? NSScreen.screens.first?.visibleFrame {
            let width = min(max(window.frame.width, window.minSize.width), targetFrame.width)
            let height = min(max(window.frame.height, window.minSize.height), targetFrame.height)
            let x = targetFrame.midX - width / 2
            let y = targetFrame.midY - height / 2
            window.setFrame(NSRect(x: x, y: y, width: width, height: height), display: true)
        } else {
            window.center()
        }
    }
}
