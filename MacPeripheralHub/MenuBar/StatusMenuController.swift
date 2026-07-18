import AppKit
import Combine

@MainActor
final class StatusMenuController: NSObject {
    private let appState: AppState
    private let openWindow: () -> Void
    private let statusItem: NSStatusItem
    private var cancellables = Set<AnyCancellable>()
    private var inventory: [DeviceViewModel] = []
    private var profiles: [ProfileViewModel] = []
    private var activeSelection: SelectionViewModel = .empty
    private var isRefreshingInventory = false
    private var deviceActions: [Int: (role: DeviceRole, deviceID: String)] = [:]
    private var profileActions: [Int: String] = [:]
    private var nextActionTag = 3000

    init(appState: AppState, openWindow: @escaping () -> Void) {
        self.appState = appState
        self.openWindow = openWindow
        self.statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        super.init()
        configureStatusItem()
        bindState()
        rebuildMenu()
    }

    private func configureStatusItem() {
        guard let button = statusItem.button else {
            return
        }

        let image = NSImage(
            systemSymbolName: "dot.radiowaves.left.and.right",
            accessibilityDescription: "MacPeripheralHub"
        )
        image?.isTemplate = true
        button.image = image
        button.toolTip = "MacPeripheralHub"
    }

    private func bindState() {
        appState.$inventory
            .combineLatest(appState.$profiles, appState.$activeSelection, appState.$isRefreshingInventory)
            .receive(on: RunLoop.main)
            .sink { [weak self] inventory, profiles, activeSelection, isRefreshingInventory in
                guard let self else {
                    return
                }

                self.inventory = inventory
                self.profiles = profiles
                self.activeSelection = activeSelection
                self.isRefreshingInventory = isRefreshingInventory
                self.rebuildMenu()
            }
            .store(in: &cancellables)
    }

    private func rebuildMenu() {
        resetActionMaps()

        let menu = NSMenu(title: "MacPeripheralHub")
        menu.autoenablesItems = false

        menu.addItem(makeHeaderItem())
        menu.addItem(makeStateItem("Mode", activeModeTitle()))
        menu.addItem(makeStateItem("Input", currentDeviceName(for: .defaultInput)))
        menu.addItem(makeStateItem("Output", currentDeviceName(for: .defaultOutput)))
        menu.addItem(makeStateItem("System", currentDeviceName(for: .systemOutput)))
        menu.addItem(.separator())

        menu.addItem(makeQuickSwitchMenuItem(title: "Input Device", role: .defaultInput))
        menu.addItem(makeQuickSwitchMenuItem(title: "Output Device", role: .defaultOutput))
        menu.addItem(makeQuickSwitchMenuItem(title: "System Output", role: .systemOutput))
        menu.addItem(.separator())

        menu.addItem(makeProfilesMenuItem())
        menu.addItem(.separator())

        menu.addItem(
            withTitle: isRefreshingInventory ? "Refreshing..." : "Refresh Devices",
            action: #selector(refreshDevices),
            keyEquivalent: ""
        ).target = self
        menu.items.last?.isEnabled = !isRefreshingInventory

        menu.addItem(
            withTitle: "Open MacPeripheralHub",
            action: #selector(openMacPeripheralHub),
            keyEquivalent: ""
        ).target = self
        menu.addItem(.separator())

        menu.addItem(
            withTitle: "Quit",
            action: #selector(quitApplication),
            keyEquivalent: "q"
        ).target = self

        statusItem.menu = menu
    }

    private func makeHeaderItem() -> NSMenuItem {
        let item = NSMenuItem(title: "MacPeripheralHub", action: nil, keyEquivalent: "")
        item.image = NSImage(systemSymbolName: "externaldrive.connected.to.line.below", accessibilityDescription: nil)
        item.isEnabled = false
        return item
    }

    private func makeStateItem(_ title: String, _ value: String) -> NSMenuItem {
        let item = NSMenuItem(title: "\(title): \(value)", action: nil, keyEquivalent: "")
        item.isEnabled = false
        return item
    }

    private func makeQuickSwitchMenuItem(title: String, role: DeviceRole) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: nil, keyEquivalent: "")
        let submenu = NSMenu(title: title)
        let devices = candidateDevices(for: role)

        if devices.isEmpty {
            let emptyItem = NSMenuItem(title: "No devices available", action: nil, keyEquivalent: "")
            emptyItem.isEnabled = false
            submenu.addItem(emptyItem)
        } else {
            for device in devices {
                let deviceItem = NSMenuItem(
                    title: pickerTitle(for: device),
                    action: #selector(selectDevice(_:)),
                    keyEquivalent: ""
                )
                deviceItem.target = self
                deviceItem.tag = nextActionTag
                deviceItem.isEnabled = device.isConnected
                deviceItem.state = isCurrentDevice(device.id, for: role) ? .on : .off
                deviceActions[deviceItem.tag] = (role, device.id)
                nextActionTag += 1
                submenu.addItem(deviceItem)
            }
        }

        item.submenu = submenu
        return item
    }

    private func makeProfilesMenuItem() -> NSMenuItem {
        let item = NSMenuItem(title: "Profiles", action: nil, keyEquivalent: "")
        let submenu = NSMenu(title: "Profiles")

        if profiles.isEmpty {
            let emptyItem = NSMenuItem(title: "No profiles yet", action: nil, keyEquivalent: "")
            emptyItem.isEnabled = false
            submenu.addItem(emptyItem)
        } else {
            for profile in profiles {
                let profileItem = NSMenuItem(
                    title: profile.name,
                    action: #selector(activateProfile(_:)),
                    keyEquivalent: ""
                )
                profileItem.target = self
                profileItem.tag = nextActionTag
                profileItem.state = isActiveProfile(profile) ? .on : .off
                profileActions[profileItem.tag] = profile.id
                nextActionTag += 1
                submenu.addItem(profileItem)
            }
        }

        item.submenu = submenu
        return item
    }

    @objc private func selectDevice(_ sender: NSMenuItem) {
        guard let action = deviceActions[sender.tag] else {
            return
        }

        appState.setManualDevice(action.deviceID, for: action.role)
    }

    @objc private func activateProfile(_ sender: NSMenuItem) {
        guard let profileID = profileActions[sender.tag] else {
            return
        }

        appState.activateProfile(id: profileID)
    }

    @objc private func refreshDevices() {
        appState.refreshInventory()
    }

    @objc private func openMacPeripheralHub() {
        openWindow()
    }

    @objc private func quitApplication() {
        NSApp.terminate(nil)
    }

    private func resetActionMaps() {
        deviceActions.removeAll()
        profileActions.removeAll()
        nextActionTag = 3000
    }

    private func activeModeTitle() -> String {
        switch activeSelection.mode {
        case .none:
            return "None"
        case .profile:
            guard let profileID = activeSelection.profileID else {
                return "Profile"
            }
            let name = profiles.first { $0.id == profileID }?.name ?? profileID
            return "Profile / \(name)"
        case .manual:
            return "Manual Control"
        }
    }

    private func currentDeviceName(for role: DeviceRole) -> String {
        if let device = currentDefaultDevice(for: role) {
            return device.displayName
        }

        if let selectedID = activeSelection.deviceID(for: role) {
            return profileDeviceName(for: selectedID)
        }

        return "Not selected"
    }

    private func currentDefaultDevice(for role: DeviceRole) -> DeviceViewModel? {
        switch role {
        case .defaultInput:
            return inventory.first { $0.isDefaultInput }
        case .defaultOutput:
            return inventory.first { $0.isDefaultOutput }
        case .systemOutput:
            return inventory.first { $0.isDefaultSystemOutput }
        case .preferredCamera:
            guard let selectedID = activeSelection.deviceID(for: role) else {
                return nil
            }
            return inventory.first { $0.id == selectedID }
        }
    }

    private func candidateDevices(for role: DeviceRole) -> [DeviceViewModel] {
        let categories: Set<DeviceCategory>
        switch role {
        case .defaultInput:
            categories = [.audioInput]
        case .defaultOutput:
            categories = [.audioOutput]
        case .systemOutput:
            categories = [.audioSystemOutput]
        case .preferredCamera:
            categories = [.camera]
        }

        return inventory
            .filter { categories.contains($0.category) }
            .sorted { left, right in
                if left.isConnected != right.isConnected {
                    return left.isConnected && !right.isConnected
                }
                if left.displayName.localizedCaseInsensitiveCompare(right.displayName) != .orderedSame {
                    return left.displayName.localizedCaseInsensitiveCompare(right.displayName) == .orderedAscending
                }
                return left.id < right.id
            }
    }

    private func isCurrentDevice(_ deviceID: String, for role: DeviceRole) -> Bool {
        currentDefaultDevice(for: role)?.id == deviceID || activeSelection.deviceID(for: role) == deviceID
    }

    private func isActiveProfile(_ profile: ProfileViewModel) -> Bool {
        activeSelection.mode == .profile && activeSelection.profileID == profile.id
    }

    private func pickerTitle(for device: DeviceViewModel) -> String {
        device.isConnected ? device.displayName : "\(device.displayName) (offline)"
    }

    private func profileDeviceName(for deviceID: String) -> String {
        guard let device = inventory.first(where: { $0.id == deviceID }) else {
            return "Missing: \(deviceID)"
        }

        return device.isConnected ? device.displayName : "\(device.displayName) (offline)"
    }
}
