import AppKit

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private let appState = AppState()
    private var mainWindowController: MainWindowController?
    private var statusMenuController: StatusMenuController?
    private var dockProfileActions: [Int: String] = [:]
    private var nextDockProfileTag = 4000
    private let didLaunchAtLogin = CommandLine.arguments.contains("--launch-at-login")

    func applicationDidFinishLaunching(_ notification: Notification) {
        writeLaunchDiagnostic("applicationDidFinishLaunching")
        configureApplicationIcon()
        NSApp.setActivationPolicy(didLaunchAtLogin ? .accessory : .regular)
        configureMainMenu()
        statusMenuController = StatusMenuController(appState: appState) { [weak self] in
            self?.showMainWindow(nil)
        }
        writeLaunchDiagnostic("statusItemConfigured")

        if didLaunchAtLogin {
            writeLaunchDiagnostic("loginLaunchHidden")
        } else {
            showMainWindow(nil)
            writeLaunchDiagnostic("mainWindowRequested")
        }

        appState.startBackgroundServices()
        appState.refreshAll()

        if !didLaunchAtLogin {
            scheduleStartupUIFailsafe()
            scheduleLaunchAtLoginPrompt()
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        appState.stopBackgroundServices()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        false
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if !flag {
            showMainWindow(nil)
        }
        return true
    }

    func applicationDockMenu(_ sender: NSApplication) -> NSMenu? {
        guard NSApp.activationPolicy() == .regular else {
            return nil
        }

        dockProfileActions.removeAll()
        nextDockProfileTag = 4000

        let menu = NSMenu(title: "MacPeripheralHub")
        menu.autoenablesItems = false

        let openItem = NSMenuItem(
            title: "Open MacPeripheralHub",
            action: #selector(showMainWindow(_:)),
            keyEquivalent: ""
        )
        openItem.target = self
        menu.addItem(openItem)
        menu.addItem(.separator())

        if appState.profiles.isEmpty {
            let emptyItem = NSMenuItem(title: "No profiles yet", action: nil, keyEquivalent: "")
            emptyItem.isEnabled = false
            menu.addItem(emptyItem)
        } else {
            for profile in appState.profiles {
                let item = NSMenuItem(
                    title: profile.name,
                    action: #selector(activateDockProfile(_:)),
                    keyEquivalent: ""
                )
                item.target = self
                item.tag = nextDockProfileTag
                item.state = isActiveProfile(profile) ? .on : .off
                dockProfileActions[item.tag] = profile.id
                nextDockProfileTag += 1
                menu.addItem(item)
            }
        }

        return menu
    }

    @objc private func showMainWindow(_ sender: Any?) {
        NSApp.setActivationPolicy(.regular)
        if mainWindowController == nil {
            mainWindowController = MainWindowController(appState: appState) { [weak self] in
                self?.hideApplicationFromDock()
            }
        }

        mainWindowController?.showAndActivate(sender)
        writeLaunchDiagnostic("mainWindowShown visible=\(mainWindowController?.window?.isVisible == true)")
        NSRunningApplication.current.activate(options: [.activateAllWindows, .activateIgnoringOtherApps])
    }

    @objc private func activateDockProfile(_ sender: NSMenuItem) {
        guard let profileID = dockProfileActions[sender.tag] else {
            return
        }

        appState.activateProfile(id: profileID)
    }

    @objc private func showAboutPanel(_ sender: Any?) {
        let version = bundleString("CFBundleShortVersionString", fallback: "1.0.0")
        let build = bundleString("CFBundleVersion", fallback: "1")
        let credits = NSAttributedString(
            string: "MacPeripheralHub keeps selected macOS audio defaults stable across device changes.",
            attributes: [
                .font: NSFont.systemFont(ofSize: 12),
                .foregroundColor: NSColor.secondaryLabelColor
            ]
        )

        var options: [NSApplication.AboutPanelOptionKey: Any] = [
            .applicationName: "MacPeripheralHub",
            .applicationVersion: version,
            .version: "Version \(version) (Build \(build))",
            .credits: credits
        ]
        options[.applicationIcon] = NSImage(named: "AppIcon") ?? NSApp.applicationIconImage

        NSApp.orderFrontStandardAboutPanel(options: options)
    }

    private func hideApplicationFromDock() {
        NSApp.setActivationPolicy(.accessory)
        writeLaunchDiagnostic("dockHiddenAfterWindowClose")
    }

    private func isActiveProfile(_ profile: ProfileViewModel) -> Bool {
        appState.activeSelection.mode == .profile && appState.activeSelection.profileID == profile.id
    }

    private func showLaunchAtLoginPromptIfNeeded() {
        appState.refreshLoginItemStatus()
        guard appState.shouldOfferLaunchAtLogin() else {
            writeLaunchDiagnostic("loginItemPromptSkipped")
            return
        }

        appState.markLaunchAtLoginPromptShown()
        writeLaunchDiagnostic("loginItemPromptShown")

        let alert = NSAlert()
        alert.messageText = "Launch MacPeripheralHub at Login?"
        alert.informativeText = "MacPeripheralHub can start automatically when you log in so it can keep your selected audio devices stable."
        alert.alertStyle = .informational
        alert.addButton(withTitle: "Enable")
        alert.addButton(withTitle: "Not Now")

        if let window = mainWindowController?.window {
            alert.beginSheetModal(for: window) { [weak self] response in
                guard response == .alertFirstButtonReturn else {
                    return
                }

                self?.appState.setLaunchAtLoginEnabled(true)
            }
        } else if alert.runModal() == .alertFirstButtonReturn {
            appState.setLaunchAtLoginEnabled(true)
        }
    }

    private func scheduleStartupUIFailsafe() {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.75) { [weak self] in
            guard let self else {
                return
            }

            if self.statusMenuController == nil {
                self.statusMenuController = StatusMenuController(appState: self.appState) { [weak self] in
                    self?.showMainWindow(nil)
                }
                self.writeLaunchDiagnostic("statusItemRecreatedByFailsafe")
            }

            if self.mainWindowController?.window?.isVisible != true {
                self.writeLaunchDiagnostic("mainWindowMissingBeforeFailsafe")
                self.showMainWindow(nil)
            } else {
                self.writeLaunchDiagnostic("mainWindowVisibleAtFailsafe")
            }
        }
    }

    private func scheduleLaunchAtLoginPrompt() {
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.25) { [weak self] in
            self?.showLaunchAtLoginPromptIfNeeded()
        }
    }

    private func configureApplicationIcon() {
        if let image = NSImage(named: "AppIcon") {
            NSApp.applicationIconImage = image
        }
    }

    private func bundleString(_ key: String, fallback: String) -> String {
        Bundle.main.object(forInfoDictionaryKey: key) as? String ?? fallback
    }

    private func configureMainMenu() {
        let mainMenu = NSMenu()

        let appMenuItem = NSMenuItem()
        mainMenu.addItem(appMenuItem)

        let appMenu = NSMenu(title: "MacPeripheralHub")
        let aboutItem = NSMenuItem(
            title: "About MacPeripheralHub",
            action: #selector(showAboutPanel(_:)),
            keyEquivalent: ""
        )
        aboutItem.target = self
        appMenu.addItem(aboutItem)
        appMenu.addItem(.separator())
        appMenu.addItem(
            withTitle: "Quit MacPeripheralHub",
            action: #selector(NSApplication.terminate(_:)),
            keyEquivalent: "q"
        )
        appMenuItem.submenu = appMenu

        let windowMenuItem = NSMenuItem()
        mainMenu.addItem(windowMenuItem)

        let windowMenu = NSMenu(title: "Window")
        windowMenu.addItem(
            withTitle: "Show MacPeripheralHub",
            action: #selector(showMainWindow(_:)),
            keyEquivalent: "0"
        )
        windowMenu.addItem(
            withTitle: "Close",
            action: #selector(NSWindow.performClose(_:)),
            keyEquivalent: "w"
        )
        windowMenuItem.submenu = windowMenu
        NSApp.windowsMenu = windowMenu

        NSApp.mainMenu = mainMenu
    }

    private func writeLaunchDiagnostic(_ message: String) {
        let fileManager = FileManager.default
        guard let supportDirectory = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            return
        }

        let appDirectory = supportDirectory.appendingPathComponent("MacPeripheralHub", isDirectory: true)
        do {
            try fileManager.createDirectory(at: appDirectory, withIntermediateDirectories: true)
            let logURL = appDirectory.appendingPathComponent("launch-diagnostics.log")
            let line = "\(Date()) \(message)\n"
            if let data = line.data(using: .utf8) {
                if fileManager.fileExists(atPath: logURL.path) {
                    let handle = try FileHandle(forWritingTo: logURL)
                    try handle.seekToEnd()
                    try handle.write(contentsOf: data)
                    try handle.close()
                } else {
                    try data.write(to: logURL)
                }
            }
        } catch {
            NSLog("MacPeripheralHub launch diagnostic write failed: \(error.localizedDescription)")
        }
    }
}
