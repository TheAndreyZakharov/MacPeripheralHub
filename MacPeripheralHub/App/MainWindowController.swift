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
        window.setFrameAutosaveName(Self.frameAutosaveName)
        if !window.setFrameUsingName(Self.frameAutosaveName) {
            window.center()
        }
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
}
