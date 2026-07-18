import AppKit

final class MainWindowController: NSWindowController, NSWindowDelegate {
    init() {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 980, height: 640),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "MacPeripheralHub"
        window.minSize = NSSize(width: 760, height: 480)
        window.center()
        window.contentView = RootView()

        super.init(window: window)
        window.delegate = self
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("MainWindowController does not support storyboard initialization.")
    }
}
