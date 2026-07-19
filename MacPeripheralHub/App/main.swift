import AppKit

@MainActor
private enum ApplicationMain {
    static let delegate = AppDelegate()
}

MainActor.assumeIsolated {
    let application = NSApplication.shared
    application.delegate = ApplicationMain.delegate
    application.setActivationPolicy(.regular)
    application.run()
}
