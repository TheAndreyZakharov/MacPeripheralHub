import AppKit

@main
@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private let appState = AppState()
    private var mainWindowController: MainWindowController?
    private var statusMenuController: StatusMenuController?
    private var dockProfileActions: [Int: String] = [:]
    private var nextDockProfileTag = 4000

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        configureMainMenu()
        statusMenuController = StatusMenuController(appState: appState) { [weak self] in
            self?.showMainWindow(nil)
        }
        showMainWindow(nil)
        appState.startBackgroundServices()
        appState.refreshAll()
        showLaunchAtLoginPromptIfNeeded()
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

        mainWindowController?.showWindow(sender)
        mainWindowController?.window?.makeKeyAndOrderFront(sender)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc private func activateDockProfile(_ sender: NSMenuItem) {
        guard let profileID = dockProfileActions[sender.tag] else {
            return
        }

        appState.activateProfile(id: profileID)
    }

    private func hideApplicationFromDock() {
        NSApp.setActivationPolicy(.accessory)
    }

    private func isActiveProfile(_ profile: ProfileViewModel) -> Bool {
        appState.activeSelection.mode == .profile && appState.activeSelection.profileID == profile.id
    }

    private func showLaunchAtLoginPromptIfNeeded() {
        appState.refreshLoginItemStatus()
        guard appState.shouldOfferLaunchAtLogin() else {
            return
        }

        appState.markLaunchAtLoginPromptShown()

        let alert = NSAlert()
        alert.messageText = "Launch MacPeripheralHub at Login?"
        alert.informativeText = "MacPeripheralHub can start automatically when you log in so it can keep your selected audio devices stable."
        alert.alertStyle = .informational
        alert.addButton(withTitle: "Enable")
        alert.addButton(withTitle: "Not Now")

        if alert.runModal() == .alertFirstButtonReturn {
            appState.setLaunchAtLoginEnabled(true)
        }
    }

    private func configureMainMenu() {
        let mainMenu = NSMenu()

        let appMenuItem = NSMenuItem()
        mainMenu.addItem(appMenuItem)

        let appMenu = NSMenu(title: "MacPeripheralHub")
        appMenu.addItem(
            withTitle: "About MacPeripheralHub",
            action: #selector(NSApplication.orderFrontStandardAboutPanel(_:)),
            keyEquivalent: ""
        )
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
}
