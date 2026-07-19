import AppKit
import Combine

private enum AppSection: Int, CaseIterable {
    case manualControl
    case devices
    case profiles
    case settings

    var title: String {
        switch self {
        case .manualControl:
            return "Manual Control"
        case .devices:
            return "Devices"
        case .profiles:
            return "Profiles"
        case .settings:
            return "Settings"
        }
    }

    var subtitle: String {
        switch self {
        case .manualControl:
            return "Current defaults and active mode"
        case .devices:
            return "Connected and remembered peripherals"
        case .profiles:
            return "Saved device combinations"
        case .settings:
            return "App behavior and startup"
        }
    }

    var symbolName: String {
        switch self {
        case .manualControl:
            return "slider.horizontal.3"
        case .devices:
            return "display.2"
        case .profiles:
            return "person.crop.rectangle.stack"
        case .settings:
            return "gearshape"
        }
    }
}

private final class FlippedDocumentView: NSView {
    override var isFlipped: Bool {
        true
    }
}

final class RootView: NSView {
    private struct Snapshot {
        var inventory: [DeviceViewModel]
        var profiles: [ProfileViewModel]
        var activeSelection: SelectionViewModel
        var isRefreshingInventory: Bool
        var loginItem: LoginItemViewModel
        var appearancePreference: AppAppearancePreference
        var error: AppErrorViewModel?

        static let empty = Snapshot(
            inventory: [],
            profiles: [],
            activeSelection: .empty,
            isRefreshingInventory: false,
            loginItem: .unknown,
            appearancePreference: .system,
            error: nil
        )
    }

    private let appState: AppState
    private var cancellables = Set<AnyCancellable>()
    private var selectedSection: AppSection = .manualControl
    private var snapshot: Snapshot = .empty

    private let sidebarView = NSView()
    private let sidebarStack = NSStackView()
    private var sectionButtons: [AppSection: NSButton] = [:]

    private let titleLabel = NSTextField(labelWithString: "")
    private let subtitleLabel = NSTextField(labelWithString: "")
    private let refreshButton = NSButton()
    private let loadingIndicator = NSProgressIndicator()
    private let errorBanner = NSView()
    private let errorLabel = NSTextField(labelWithString: "")
    private let deviceToolbar = NSStackView()
    private let deviceSearchField = NSSearchField()
    private let deviceExpandAllButton = NSButton(checkboxWithTitle: "Expand all", target: nil, action: nil)
    private let deviceHideOfflineButton = NSButton(checkboxWithTitle: "Hide offline", target: nil, action: nil)
    private let deviceSummaryLabel = NSTextField(labelWithString: "")
    private let deviceRefreshButton = NSButton()
    private let contentStack = NSStackView()
    private var deviceFilterText = ""
    private var collapsedDeviceCategories = Set<DeviceCategory>()
    private var hideOfflineDevices = false
    private var deviceCategoryTags: [Int: DeviceCategory] = [:]
    private var nextDeviceCategoryTag = 3000
    private var profileDraftID: String?
    private var profileDraftName = ""
    private var profileDraftRoleDeviceIDs: [DeviceRole: String] = [:]
    private var profileDraftEnforceAudioDefaults = true
    private var profileNameField: NSTextField?
    private var profileEnforceButton: NSButton?
    private var profileRolePickers: [DeviceRole: NSPopUpButton] = [:]
    private var profileActionProfileIDs: [Int: String] = [:]
    private var profilePickerRoles: [Int: DeviceRole] = [:]
    private var nextProfileActionTag = 2000

    init(appState: AppState, frame frameRect: NSRect = .zero) {
        self.appState = appState
        super.init(frame: frameRect)
        configureView()
        configureLayout()
        bindState()
        render()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("RootView does not support storyboard initialization.")
    }

    override func viewDidChangeEffectiveAppearance() {
        super.viewDidChangeEffectiveAppearance()
        updateDynamicColors()
        render()
    }

    private func configureView() {
        wantsLayer = true
        updateDynamicColors()

        titleLabel.font = NSFont.systemFont(ofSize: 24, weight: .semibold)
        titleLabel.textColor = .labelColor
        titleLabel.lineBreakMode = .byTruncatingTail

        subtitleLabel.font = NSFont.systemFont(ofSize: 13, weight: .regular)
        subtitleLabel.textColor = .secondaryLabelColor
        subtitleLabel.lineBreakMode = .byTruncatingTail

        refreshButton.bezelStyle = .toolbar
        refreshButton.image = NSImage(systemSymbolName: "arrow.clockwise", accessibilityDescription: "Refresh")
        refreshButton.target = self
        refreshButton.action = #selector(refreshInventory)
        refreshButton.toolTip = "Refresh inventory"

        loadingIndicator.style = .spinning
        loadingIndicator.controlSize = .small
        loadingIndicator.isDisplayedWhenStopped = false

        errorBanner.wantsLayer = true
        errorBanner.layer?.backgroundColor = dynamicCGColor(NSColor.systemRed.withAlphaComponent(0.08))
        errorBanner.layer?.cornerRadius = 6
        errorBanner.isHidden = true

        errorLabel.font = NSFont.systemFont(ofSize: 13, weight: .regular)
        errorLabel.textColor = .systemRed
        errorLabel.maximumNumberOfLines = 2

        deviceSearchField.placeholderString = "Search devices"
        deviceSearchField.target = self
        deviceSearchField.action = #selector(deviceFilterChanged(_:))
        deviceSearchField.sendsSearchStringImmediately = true
        deviceSearchField.translatesAutoresizingMaskIntoConstraints = false
        deviceSearchField.widthAnchor.constraint(greaterThanOrEqualToConstant: 240).isActive = true

        deviceExpandAllButton.target = self
        deviceExpandAllButton.action = #selector(expandAllDeviceCategoriesChanged(_:))
        deviceExpandAllButton.toolTip = "Expand or collapse all device categories"
        deviceExpandAllButton.font = NSFont.systemFont(ofSize: 12, weight: .regular)

        deviceHideOfflineButton.target = self
        deviceHideOfflineButton.action = #selector(hideOfflineDevicesChanged(_:))
        deviceHideOfflineButton.toolTip = "Hide remembered devices that are currently offline"
        deviceHideOfflineButton.font = NSFont.systemFont(ofSize: 12, weight: .regular)

        deviceSummaryLabel.font = NSFont.systemFont(ofSize: 12, weight: .regular)
        deviceSummaryLabel.textColor = .secondaryLabelColor
        deviceSummaryLabel.lineBreakMode = .byTruncatingTail

        deviceRefreshButton.bezelStyle = .toolbar
        deviceRefreshButton.image = NSImage(systemSymbolName: "arrow.clockwise", accessibilityDescription: "Refresh")
        deviceRefreshButton.target = self
        deviceRefreshButton.action = #selector(refreshInventory)
        deviceRefreshButton.toolTip = "Refresh inventory"

        deviceToolbar.orientation = .horizontal
        deviceToolbar.alignment = .centerY
        deviceToolbar.spacing = 10
        deviceToolbar.edgeInsets = NSEdgeInsets(top: 10, left: 12, bottom: 10, right: 12)
        deviceToolbar.wantsLayer = true
        deviceToolbar.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        deviceToolbar.layer?.cornerRadius = 6
        deviceToolbar.addArrangedSubview(deviceSearchField)
        deviceToolbar.addArrangedSubview(deviceExpandAllButton)
        deviceToolbar.addArrangedSubview(deviceHideOfflineButton)
        deviceToolbar.addArrangedSubview(NSView())
        deviceToolbar.addArrangedSubview(deviceSummaryLabel)
        deviceToolbar.addArrangedSubview(deviceRefreshButton)

        sidebarStack.orientation = .vertical
        sidebarStack.alignment = .width
        sidebarStack.spacing = 6
        sidebarStack.translatesAutoresizingMaskIntoConstraints = false

        contentStack.orientation = .vertical
        contentStack.alignment = .width
        contentStack.spacing = 10
        contentStack.edgeInsets = NSEdgeInsets(top: 16, left: 22, bottom: 24, right: 22)
    }

    private func updateDynamicColors() {
        layer?.backgroundColor = dynamicCGColor(.windowBackgroundColor)
        sidebarView.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        deviceToolbar.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        errorBanner.layer?.backgroundColor = dynamicCGColor(NSColor.systemRed.withAlphaComponent(0.08))
    }

    private func dynamicCGColor(_ color: NSColor) -> CGColor {
        var resolvedColor = color.cgColor
        effectiveAppearance.performAsCurrentDrawingAppearance {
            resolvedColor = color.cgColor
        }
        return resolvedColor
    }

    private func configureLayout() {
        let rootStack = NSStackView()
        rootStack.orientation = .horizontal
        rootStack.alignment = .height
        rootStack.spacing = 0
        rootStack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(rootStack)

        sidebarView.wantsLayer = true
        sidebarView.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        sidebarView.addSubview(sidebarStack)
        rootStack.addArrangedSubview(sidebarView)

        let appNameLabel = NSTextField(labelWithString: "MacPeripheralHub")
        appNameLabel.font = NSFont.systemFont(ofSize: 17, weight: .semibold)
        appNameLabel.textColor = .labelColor
        appNameLabel.lineBreakMode = .byTruncatingTail
        sidebarStack.addArrangedSubview(appNameLabel)
        sidebarStack.setCustomSpacing(18, after: appNameLabel)

        for section in AppSection.allCases {
            let button = makeSidebarButton(for: section)
            sectionButtons[section] = button
            sidebarStack.addArrangedSubview(button)
        }

        let spacer = NSView()
        spacer.translatesAutoresizingMaskIntoConstraints = false
        sidebarStack.addArrangedSubview(spacer)

        let contentContainer = NSView()
        contentContainer.translatesAutoresizingMaskIntoConstraints = false
        rootStack.addArrangedSubview(contentContainer)

        let headerStack = makeHeaderStack()
        let scrollView = makeScrollView()
        let contentLayout = NSStackView(views: [headerStack, errorBanner, scrollView])
        contentLayout.orientation = .vertical
        contentLayout.alignment = .width
        contentLayout.spacing = 0
        contentLayout.translatesAutoresizingMaskIntoConstraints = false
        contentContainer.addSubview(contentLayout)

        NSLayoutConstraint.activate([
            rootStack.leadingAnchor.constraint(equalTo: leadingAnchor),
            rootStack.trailingAnchor.constraint(equalTo: trailingAnchor),
            rootStack.topAnchor.constraint(equalTo: topAnchor),
            rootStack.bottomAnchor.constraint(equalTo: bottomAnchor),

            sidebarView.widthAnchor.constraint(equalToConstant: 220),
            sidebarStack.leadingAnchor.constraint(equalTo: sidebarView.leadingAnchor, constant: 14),
            sidebarStack.trailingAnchor.constraint(equalTo: sidebarView.trailingAnchor, constant: -14),
            sidebarStack.topAnchor.constraint(equalTo: sidebarView.topAnchor, constant: 24),
            sidebarStack.bottomAnchor.constraint(equalTo: sidebarView.bottomAnchor, constant: -16),
            spacer.heightAnchor.constraint(greaterThanOrEqualToConstant: 1),

            contentLayout.leadingAnchor.constraint(equalTo: contentContainer.leadingAnchor),
            contentLayout.trailingAnchor.constraint(equalTo: contentContainer.trailingAnchor),
            contentLayout.topAnchor.constraint(equalTo: contentContainer.topAnchor),
            contentLayout.bottomAnchor.constraint(equalTo: contentContainer.bottomAnchor),
            contentContainer.widthAnchor.constraint(greaterThanOrEqualToConstant: 540),

            errorBanner.heightAnchor.constraint(greaterThanOrEqualToConstant: 42)
        ])
    }

    private func makeHeaderStack() -> NSView {
        let titleStack = NSStackView(views: [titleLabel, subtitleLabel])
        titleStack.orientation = .vertical
        titleStack.alignment = .leading
        titleStack.spacing = 3

        let actionsStack = NSStackView(views: [loadingIndicator, refreshButton])
        actionsStack.orientation = .horizontal
        actionsStack.alignment = .centerY
        actionsStack.spacing = 8

        let headerStack = NSStackView(views: [titleStack, NSView(), actionsStack])
        headerStack.orientation = .horizontal
        headerStack.alignment = .centerY
        headerStack.spacing = 12
        headerStack.edgeInsets = NSEdgeInsets(top: 22, left: 24, bottom: 16, right: 24)
        return headerStack
    }

    private func makeScrollView() -> NSScrollView {
        let documentView = FlippedDocumentView()
        documentView.translatesAutoresizingMaskIntoConstraints = false
        documentView.addSubview(contentStack)

        let scrollView = NSScrollView()
        scrollView.drawsBackground = false
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.documentView = documentView

        contentStack.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            contentStack.leadingAnchor.constraint(equalTo: documentView.leadingAnchor),
            contentStack.trailingAnchor.constraint(equalTo: documentView.trailingAnchor),
            contentStack.topAnchor.constraint(equalTo: documentView.topAnchor),
            contentStack.bottomAnchor.constraint(lessThanOrEqualTo: documentView.bottomAnchor),
            contentStack.widthAnchor.constraint(equalTo: scrollView.contentView.widthAnchor),
            documentView.widthAnchor.constraint(equalTo: scrollView.contentView.widthAnchor),
            documentView.heightAnchor.constraint(greaterThanOrEqualTo: scrollView.contentView.heightAnchor)
        ])

        return scrollView
    }

    private func makeSidebarButton(for section: AppSection) -> NSButton {
        let button = NSButton(title: section.title, target: self, action: #selector(selectSection(_:)))
        button.image = NSImage(systemSymbolName: section.symbolName, accessibilityDescription: section.title)
        button.imagePosition = .imageLeading
        button.alignment = .left
        button.bezelStyle = .regularSquare
        button.isBordered = false
        button.tag = section.rawValue
        button.font = NSFont.systemFont(ofSize: 13, weight: .medium)
        button.contentTintColor = .labelColor
        button.wantsLayer = true
        button.layer?.cornerRadius = 6
        button.translatesAutoresizingMaskIntoConstraints = false
        button.heightAnchor.constraint(equalToConstant: 34).isActive = true
        return button
    }

    private func bindState() {
        appState.$inventory
            .combineLatest(appState.$profiles, appState.$activeSelection, appState.$isRefreshingInventory)
            .receive(on: RunLoop.main)
            .sink { [weak self] inventory, profiles, activeSelection, isRefreshingInventory in
                guard let self else {
                    return
                }

                snapshot.inventory = inventory
                snapshot.profiles = profiles
                snapshot.activeSelection = activeSelection
                snapshot.isRefreshingInventory = isRefreshingInventory
                render()
            }
            .store(in: &cancellables)

        appState.$error
            .receive(on: RunLoop.main)
            .sink { [weak self] error in
                self?.snapshot.error = error
                self?.render()
            }
            .store(in: &cancellables)

        appState.$loginItem
            .receive(on: RunLoop.main)
            .sink { [weak self] loginItem in
                self?.snapshot.loginItem = loginItem
                self?.render()
            }
            .store(in: &cancellables)

        appState.$appearancePreference
            .receive(on: RunLoop.main)
            .sink { [weak self] appearancePreference in
                self?.snapshot.appearancePreference = appearancePreference
                self?.render()
            }
            .store(in: &cancellables)
    }

    @objc private func selectSection(_ sender: NSButton) {
        guard let section = AppSection(rawValue: sender.tag) else {
            return
        }

        selectedSection = section
        render()
    }

    @objc private func refreshInventory() {
        appState.refreshInventory()
    }

    @objc private func loginItemToggleChanged(_ sender: NSButton) {
        appState.setLaunchAtLoginEnabled(sender.state == .on)
    }

    @objc private func refreshLoginItemStatus() {
        appState.refreshLoginItemStatus()
    }

    @objc private func appearancePreferenceChanged(_ sender: NSPopUpButton) {
        guard let rawValue = representedString(from: sender.selectedItem),
              let preference = AppAppearancePreference(rawValue: rawValue) else {
            return
        }

        appState.setAppearancePreference(preference)
    }

    @objc private func deviceFilterChanged(_ sender: NSSearchField) {
        deviceFilterText = sender.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        render()
    }

    @objc private func expandAllDeviceCategoriesChanged(_ sender: NSButton) {
        let categoriesWithDevices = categoriesVisibleInDeviceList()
        if sender.state == .on {
            collapsedDeviceCategories.subtract(categoriesWithDevices)
        } else {
            collapsedDeviceCategories.formUnion(categoriesWithDevices)
        }
        render()
    }

    @objc private func hideOfflineDevicesChanged(_ sender: NSButton) {
        hideOfflineDevices = sender.state == .on
        render()
    }

    @objc private func toggleDeviceCategory(_ sender: NSButton) {
        guard let category = deviceCategoryTags[sender.tag] else {
            return
        }

        if collapsedDeviceCategories.contains(category) {
            collapsedDeviceCategories.remove(category)
        } else {
            collapsedDeviceCategories.insert(category)
        }
        render()
    }

    private func render() {
        updateDynamicColors()
        titleLabel.stringValue = selectedSection.title
        subtitleLabel.stringValue = selectedSection.subtitle
        refreshButton.isEnabled = !snapshot.isRefreshingInventory
        deviceRefreshButton.isEnabled = !snapshot.isRefreshingInventory

        if snapshot.isRefreshingInventory {
            loadingIndicator.startAnimation(nil)
        } else {
            loadingIndicator.stopAnimation(nil)
        }

        for (section, button) in sectionButtons {
            let isSelected = section == selectedSection
            button.state = isSelected ? .on : .off
            button.contentTintColor = isSelected ? .controlAccentColor : .labelColor
            button.layer?.backgroundColor = isSelected
                ? dynamicCGColor(NSColor.controlAccentColor.withAlphaComponent(0.12))
                : dynamicCGColor(.clear)
        }

        renderError()
        replaceContent(with: makeContentViews())
    }

    private func renderError() {
        guard let error = snapshot.error else {
            errorBanner.isHidden = true
            return
        }

        if errorLabel.superview == nil {
            let errorIcon = NSImageView(
                image: NSImage(systemSymbolName: "exclamationmark.triangle.fill", accessibilityDescription: "Error") ??
                    NSImage()
            )
            errorIcon.symbolConfiguration = .init(pointSize: 14, weight: .semibold)
            errorIcon.contentTintColor = .systemRed
            errorIcon.setContentHuggingPriority(.required, for: .horizontal)

            let dismissButton = NSButton()
            dismissButton.image = NSImage(systemSymbolName: "xmark", accessibilityDescription: "Dismiss")
            dismissButton.bezelStyle = .toolbar
            dismissButton.target = self
            dismissButton.action = #selector(dismissError)
            dismissButton.toolTip = "Dismiss error"

            let errorStack = NSStackView(views: [errorIcon, errorLabel, NSView(), dismissButton])
            errorStack.orientation = .horizontal
            errorStack.alignment = .centerY
            errorStack.spacing = 10
            errorStack.edgeInsets = NSEdgeInsets(top: 10, left: 24, bottom: 10, right: 20)
            errorStack.translatesAutoresizingMaskIntoConstraints = false
            errorBanner.addSubview(errorStack)

            NSLayoutConstraint.activate([
                errorStack.leadingAnchor.constraint(equalTo: errorBanner.leadingAnchor),
                errorStack.trailingAnchor.constraint(equalTo: errorBanner.trailingAnchor),
                errorStack.topAnchor.constraint(equalTo: errorBanner.topAnchor),
                errorStack.bottomAnchor.constraint(equalTo: errorBanner.bottomAnchor)
            ])
        }

        errorLabel.stringValue = "\(error.operation): \(error.message)"
        errorBanner.isHidden = false
    }

    @objc private func dismissError() {
        appState.dismissError()
    }

    private func replaceContent(with views: [NSView]) {
        for view in contentStack.arrangedSubviews {
            contentStack.removeArrangedSubview(view)
            view.removeFromSuperview()
        }

        for view in views {
            contentStack.addArrangedSubview(view)
        }
    }

    private func makeContentViews() -> [NSView] {
        switch selectedSection {
        case .manualControl:
            return makeManualViews()
        case .devices:
            return makeDeviceViews()
        case .profiles:
            return makeProfileViews()
        case .settings:
            return makeSettingsViews()
        }
    }

    private func makeManualViews() -> [NSView] {
        if snapshot.isRefreshingInventory && snapshot.inventory.isEmpty {
            return [
                makeStateView(
                    title: "Loading devices",
                    detail: "Scanning connected peripherals and current system defaults.",
                    symbolName: "arrow.triangle.2.circlepath"
                )
            ]
        }

        var views: [NSView] = [
            makeMetricRow(
                title: "Active Mode",
                value: activeModeTitle(),
                detail: activeProfileName()
            )
        ]

        views.append(makeSectionHeader(title: "Current System Defaults", count: 3))
        views.append(makeCurrentDefaultRow(for: .defaultInput))
        views.append(makeCurrentDefaultRow(for: .defaultOutput))
        views.append(makeCurrentDefaultRow(for: .systemOutput))

        views.append(makeSectionHeader(title: "Manual Selection", count: DeviceRole.allCases.count))
        views.append(contentsOf: DeviceRole.allCases.map(makeManualPickerRow))

        return views
    }

    private func makeCurrentDefaultRow(for role: DeviceRole) -> NSView {
        let currentDevice = currentDefaultDevice(for: role)
        let selectedID = snapshot.activeSelection.deviceID(for: role)
        let fallbackDetail = selectedID.map { selectedDeviceDetail(for: $0) } ?? "No active device"

        return makeMetricRow(
            title: role.title,
            value: currentDevice?.displayName ?? "Not selected",
            detail: currentDevice.map(deviceDetail) ?? fallbackDetail
        )
    }

    private func makeManualPickerRow(for role: DeviceRole) -> NSView {
        let roleLabel = makeLabel(role.title, size: 13, weight: .medium, color: .secondaryLabelColor)
        let currentLabel = makeLabel(currentRoleSummary(for: role), size: 12, weight: .regular, color: .tertiaryLabelColor)
        currentLabel.lineBreakMode = .byTruncatingMiddle

        let labelStack = NSStackView(views: [roleLabel, currentLabel])
        labelStack.orientation = .vertical
        labelStack.alignment = .leading
        labelStack.spacing = 2

        let picker = NSPopUpButton()
        picker.bezelStyle = .rounded
        picker.target = self
        picker.action = #selector(manualPickerChanged(_:))
        picker.tag = tag(for: role)
        picker.toolTip = role.title
        picker.translatesAutoresizingMaskIntoConstraints = false
        picker.widthAnchor.constraint(greaterThanOrEqualToConstant: 280).isActive = true

        populatePicker(picker, for: role)

        let row = NSStackView(views: [labelStack, NSView(), picker])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 10, left: 12, bottom: 10, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        row.layer?.cornerRadius = 6
        return row
    }

    private func populatePicker(_ picker: NSPopUpButton, for role: DeviceRole) {
        picker.removeAllItems()

        let selectedID = snapshot.activeSelection.deviceID(for: role)
        let currentID = currentDefaultDevice(for: role)?.id
        let preferredID = selectedID ?? currentID
        let candidates = candidateDevices(for: role)

        if candidates.isEmpty && preferredID == nil {
            picker.addItem(withTitle: "No devices available")
            picker.lastItem?.isEnabled = false
            return
        }

        picker.addItem(withTitle: "Not selected")
        picker.lastItem?.isEnabled = false

        var knownIDs = Set<String>()
        for device in candidates {
            knownIDs.insert(device.id)
            picker.addItem(withTitle: pickerTitle(for: device))
            picker.lastItem?.representedObject = device.id
            picker.lastItem?.isEnabled = device.isConnected
        }

        if let preferredID, !knownIDs.contains(preferredID) {
            picker.addItem(withTitle: "Missing: \(preferredID)")
            picker.lastItem?.representedObject = preferredID
            picker.lastItem?.isEnabled = false
        }

        if let preferredID,
           let item = picker.itemArray.first(where: { representedString(from: $0) == preferredID }) {
            picker.select(item)
        } else {
            picker.selectItem(at: 0)
        }
    }

    @objc private func manualPickerChanged(_ sender: NSPopUpButton) {
        guard let role = role(for: sender.tag),
              let selectedID = representedString(from: sender.selectedItem),
              let selectedDevice = device(namedBy: selectedID),
              selectedDevice.isConnected else {
            return
        }

        appState.setManualDevice(selectedID, for: role)
    }

    private func makeSettingsViews() -> [NSView] {
        [
            makeSectionHeader(title: "Appearance", count: 1),
            makeAppearanceSettingsRow(),
            makeSectionHeader(title: "Startup", count: 1),
            makeLoginItemSettingsRow()
        ]
    }

    private func makeAppearanceSettingsRow() -> NSView {
        let title = makeLabel("Theme", size: 13, weight: .medium, color: .labelColor)
        let detail = makeLabel(snapshot.appearancePreference.detail, size: 12, weight: .regular, color: .secondaryLabelColor)
        detail.maximumNumberOfLines = 2

        let picker = NSPopUpButton()
        picker.bezelStyle = .rounded
        picker.target = self
        picker.action = #selector(appearancePreferenceChanged(_:))
        picker.toolTip = "Choose app appearance"
        picker.translatesAutoresizingMaskIntoConstraints = false
        picker.widthAnchor.constraint(greaterThanOrEqualToConstant: 160).isActive = true

        for preference in AppAppearancePreference.allCases {
            picker.addItem(withTitle: preference.title)
            picker.lastItem?.representedObject = preference.rawValue
        }

        if let item = picker.itemArray.first(where: { representedString(from: $0) == snapshot.appearancePreference.rawValue }) {
            picker.select(item)
        }

        let textStack = NSStackView(views: [title, detail])
        textStack.orientation = .vertical
        textStack.alignment = .leading
        textStack.spacing = 3

        let row = NSStackView(views: [textStack, NSView(), picker])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        row.layer?.cornerRadius = 6
        return row
    }

    private func makeLoginItemSettingsRow() -> NSView {
        let title = makeLabel("Launch at Login", size: 13, weight: .medium, color: .labelColor)
        let detail = makeLabel(snapshot.loginItem.status.detail, size: 12, weight: .regular, color: .secondaryLabelColor)
        detail.maximumNumberOfLines = 2

        let status = makeStatusLabel(snapshot.loginItem.status.title)
        let toggle = NSButton(
            checkboxWithTitle: "",
            target: self,
            action: #selector(loginItemToggleChanged(_:))
        )
        toggle.state = snapshot.loginItem.isEnabled ? .on : .off
        toggle.isEnabled = snapshot.loginItem.canToggle
        toggle.toolTip = "Launch MacPeripheralHub when you log in"

        let refreshButton = makeActionButton(
            title: "Refresh",
            symbolName: "arrow.clockwise",
            action: #selector(refreshLoginItemStatus)
        )

        let textStack = NSStackView(views: [title, detail])
        textStack.orientation = .vertical
        textStack.alignment = .leading
        textStack.spacing = 3

        let actions = NSStackView(views: [status, toggle, refreshButton])
        actions.orientation = .horizontal
        actions.alignment = .centerY
        actions.spacing = 8

        let row = NSStackView(views: [textStack, NSView(), actions])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        row.layer?.cornerRadius = 6
        return row
    }

    private func makeDeviceViews() -> [NSView] {
        deviceCategoryTags.removeAll()
        nextDeviceCategoryTag = 3000

        let searchedDevices = searchedInventory()
        let filteredDevices = filteredInventory()
        var views: [NSView] = [
            makeDeviceToolbar(
                total: snapshot.inventory.count,
                searched: searchedDevices.count,
                visible: filteredDevices.count,
                hiddenOffline: hideOfflineDevices ? searchedDevices.filter { !$0.isConnected }.count : 0
            )
        ]

        if snapshot.isRefreshingInventory && snapshot.inventory.isEmpty {
            views.append(
                makeStateView(
                    title: "Loading devices",
                    detail: "Scanning connected peripherals and current system defaults.",
                    symbolName: "arrow.triangle.2.circlepath"
                )
            )
            return views
        }

        if snapshot.inventory.isEmpty {
            views.append(
                makeStateView(
                    title: "No devices found",
                    detail: "Connect a peripheral or refresh when macOS finishes detecting devices.",
                    symbolName: "externaldrive.badge.xmark"
                )
            )
            return views
        }

        if filteredDevices.isEmpty {
            let detail = hideOfflineDevices
                ? "No online devices match the current device filters."
                : "Nothing matches \"\(deviceFilterText)\"."
            views.append(
                makeStateView(
                    title: "No matching devices",
                    detail: detail,
                    symbolName: "magnifyingglass"
                )
            )
            return views
        }

        for category in DeviceCategory.allCases {
            let searchedCategoryDevices = searchedDevices.filter { $0.category == category }
            let visibleCategoryDevices = filteredDevices.filter { $0.category == category }
            guard !visibleCategoryDevices.isEmpty else {
                continue
            }

            views.append(
                makeDeviceCategoryHeader(
                    category: category,
                    visibleCount: visibleCategoryDevices.count,
                    matchingCount: searchedCategoryDevices.count,
                    hiddenOfflineCount: hideOfflineDevices ? searchedCategoryDevices.filter { !$0.isConnected }.count : 0
                )
            )

            guard !collapsedDeviceCategories.contains(category) else {
                continue
            }

            for device in visibleCategoryDevices {
                views.append(makeDeviceRow(device))
            }
        }
        return views
    }

    private func makeDeviceToolbar(total: Int, searched: Int, visible: Int, hiddenOffline: Int) -> NSView {
        if deviceSearchField.stringValue != deviceFilterText {
            deviceSearchField.stringValue = deviceFilterText
        }

        deviceHideOfflineButton.state = hideOfflineDevices ? .on : .off
        deviceExpandAllButton.state = categoriesVisibleInDeviceList().allSatisfy {
            !collapsedDeviceCategories.contains($0)
        } ? .on : .off

        if deviceFilterText.isEmpty && hiddenOffline == 0 {
            deviceSummaryLabel.stringValue = "\(total) devices"
        } else if hiddenOffline > 0 {
            deviceSummaryLabel.stringValue = "\(visible) shown / \(hiddenOffline) offline hidden / \(total) total"
        } else {
            deviceSummaryLabel.stringValue = "\(visible) shown / \(searched) matching / \(total) total"
        }

        return deviceToolbar
    }

    private func makeProfileViews() -> [NSView] {
        resetProfileActionMaps()
        var views = [makeProfileToolbar()]

        if profileDraftID != nil {
            views.append(makeProfileEditorView())
        }

        if snapshot.profiles.isEmpty {
            views.append(
                makeStateView(
                    title: "No profiles yet",
                    detail: "Create a profile to save a stable combination of input, output, system output and camera preferences.",
                    symbolName: "person.crop.rectangle.stack"
                )
            )
            return views
        }

        views.append(makeSectionHeader(title: "Saved Profiles", count: snapshot.profiles.count))
        views.append(contentsOf: snapshot.profiles.map { profile in
            makeProfileRow(profile)
        })
        return views
    }

    private func makeProfileToolbar() -> NSView {
        let total = snapshot.profiles.count
        let activeName = activeProfileDisplayName()
        let summary = makeLabel("\(total) profiles / active: \(activeName)", size: 12, weight: .regular, color: .secondaryLabelColor)
        summary.lineBreakMode = .byTruncatingTail

        let newButton = makeActionButton(title: "New Profile", symbolName: "plus", action: #selector(beginCreateProfile))
        newButton.isEnabled = profileDraftID == nil

        let row = NSStackView(views: [summary, NSView(), newButton])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 10, left: 12, bottom: 10, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        row.layer?.cornerRadius = 6
        return row
    }

    private func makeProfileEditorView() -> NSView {
        profileRolePickers.removeAll()

        let title = makeLabel(profileEditorTitle(), size: 15, weight: .semibold, color: .labelColor)

        let nameField = NSTextField(string: profileDraftName)
        nameField.placeholderString = "Profile name"
        nameField.target = self
        nameField.action = #selector(profileNameSubmitted(_:))
        nameField.translatesAutoresizingMaskIntoConstraints = false
        nameField.widthAnchor.constraint(greaterThanOrEqualToConstant: 260).isActive = true
        profileNameField = nameField

        let nameRow = makeProfileFormRow(title: "Name", control: nameField)

        let enforceButton = NSButton(checkboxWithTitle: "Enforce audio defaults", target: self, action: #selector(profileEnforceChanged(_:)))
        enforceButton.state = profileDraftEnforceAudioDefaults ? .on : .off
        enforceButton.toolTip = "Keep selected audio devices as system defaults while this profile is active"
        profileEnforceButton = enforceButton

        let enforceRow = makeProfileFormRow(title: "Behavior", control: enforceButton)

        var rows: [NSView] = [title, nameRow, enforceRow]
        for role in DeviceRole.allCases {
            rows.append(makeProfileRolePickerRow(for: role))
        }

        let saveButton = makeActionButton(title: "Save", symbolName: "checkmark", action: #selector(saveProfileDraft))
        saveButton.keyEquivalent = "\r"

        let cancelButton = makeActionButton(title: "Cancel", symbolName: "xmark", action: #selector(cancelProfileEditing))
        let actions = NSStackView(views: [NSView(), cancelButton, saveButton])
        actions.orientation = .horizontal
        actions.alignment = .centerY
        actions.spacing = 8
        rows.append(actions)

        let stack = NSStackView(views: rows)
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 10
        stack.edgeInsets = NSEdgeInsets(top: 14, left: 12, bottom: 14, right: 12)
        stack.wantsLayer = true
        stack.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        stack.layer?.cornerRadius = 6
        return stack
    }

    private func makeProfileRolePickerRow(for role: DeviceRole) -> NSView {
        let picker = NSPopUpButton()
        picker.bezelStyle = .rounded
        picker.target = self
        picker.action = #selector(profilePickerChanged(_:))
        picker.tag = nextProfileActionTag
        picker.toolTip = role.title
        picker.translatesAutoresizingMaskIntoConstraints = false
        picker.widthAnchor.constraint(greaterThanOrEqualToConstant: 300).isActive = true
        profilePickerRoles[picker.tag] = role
        nextProfileActionTag += 1

        populateProfilePicker(picker, for: role)
        profileRolePickers[role] = picker

        return makeProfileFormRow(title: role.title, control: picker)
    }

    private func makeProfileFormRow(title: String, control: NSView) -> NSView {
        let titleLabel = makeLabel(title, size: 13, weight: .medium, color: .secondaryLabelColor)

        let row = NSStackView(views: [titleLabel, NSView(), control])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 4, left: 0, bottom: 4, right: 0)
        return row
    }

    private func makeProfileRow(_ profile: ProfileViewModel) -> NSView {
        let selected = snapshot.activeSelection.mode == .profile &&
            snapshot.activeSelection.profileID == profile.id
        let roleCount = profile.selection.roleDeviceIDs.count

        let title = makeLabel(profile.name, size: 13, weight: .medium, color: .labelColor)
        title.lineBreakMode = .byTruncatingTail

        let subtitle = makeLabel(profileSummary(for: profile), size: 12, weight: .regular, color: .secondaryLabelColor)
        subtitle.lineBreakMode = .byTruncatingTail

        let detail = makeLabel(profileRoleSummary(for: profile), size: 11, weight: .regular, color: .tertiaryLabelColor)
        detail.maximumNumberOfLines = 4
        detail.lineBreakMode = .byTruncatingTail

        let textStack = NSStackView(views: [title, subtitle, detail])
        textStack.orientation = .vertical
        textStack.alignment = .leading
        textStack.spacing = 3

        let activeLabel = makeStatusLabel(selected ? "Active" : "\(roleCount) selected")
        let activateButton = makeProfileActionButton(
            title: "Activate",
            symbolName: "play.fill",
            profileID: profile.id,
            action: #selector(activateProfileFromButton(_:))
        )
        activateButton.isEnabled = !selected

        let editButton = makeProfileActionButton(
            title: "Edit",
            symbolName: "pencil",
            profileID: profile.id,
            action: #selector(beginEditProfile(_:))
        )
        editButton.isEnabled = profileDraftID == nil

        let deleteButton = makeProfileActionButton(
            title: "Delete",
            symbolName: "trash",
            profileID: profile.id,
            action: #selector(deleteProfileFromButton(_:))
        )
        deleteButton.isEnabled = profileDraftID == nil

        let actions = NSStackView(views: [activeLabel, activateButton, editButton, deleteButton])
        actions.orientation = .horizontal
        actions.alignment = .centerY
        actions.spacing = 6

        let row = NSStackView(views: [textStack, NSView(), actions])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 10, left: 12, bottom: 10, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = selected
            ? dynamicCGColor(NSColor.controlAccentColor.withAlphaComponent(0.12))
            : dynamicCGColor(.controlBackgroundColor)
        row.layer?.cornerRadius = 6
        return row
    }

    private func makeActionButton(title: String, symbolName: String, action: Selector) -> NSButton {
        let button = NSButton(title: title, target: self, action: action)
        button.bezelStyle = .rounded
        button.image = NSImage(systemSymbolName: symbolName, accessibilityDescription: title)
        button.imagePosition = .imageLeading
        button.font = NSFont.systemFont(ofSize: 12, weight: .medium)
        button.toolTip = title
        return button
    }

    private func makeProfileActionButton(
        title: String,
        symbolName: String,
        profileID: String,
        action: Selector
    ) -> NSButton {
        let button = makeActionButton(title: title, symbolName: symbolName, action: action)
        button.tag = nextProfileActionTag
        profileActionProfileIDs[button.tag] = profileID
        nextProfileActionTag += 1
        return button
    }

    private func populateProfilePicker(_ picker: NSPopUpButton, for role: DeviceRole) {
        picker.removeAllItems()

        let selectedID = profileDraftRoleDeviceIDs[role]
        let candidates = candidateDevices(for: role)

        picker.addItem(withTitle: "Not selected")
        picker.lastItem?.representedObject = ""

        var knownIDs = Set<String>()
        for device in candidates {
            knownIDs.insert(device.id)
            picker.addItem(withTitle: pickerTitle(for: device))
            picker.lastItem?.representedObject = device.id
            picker.lastItem?.isEnabled = device.isConnected
        }

        if let selectedID, !selectedID.isEmpty, !knownIDs.contains(selectedID) {
            picker.addItem(withTitle: "Missing: \(selectedID)")
            picker.lastItem?.representedObject = selectedID
            picker.lastItem?.isEnabled = true
        }

        if let selectedID,
           let item = picker.itemArray.first(where: { representedString(from: $0) == selectedID }) {
            picker.select(item)
        } else {
            picker.selectItem(at: 0)
        }
    }

    @objc private func beginCreateProfile() {
        let existingNames = Set(snapshot.profiles.map { $0.name })
        var index = snapshot.profiles.count + 1
        var candidateName = "New Profile \(index)"
        while existingNames.contains(candidateName) {
            index += 1
            candidateName = "New Profile \(index)"
        }

        profileDraftID = UUID().uuidString
        profileDraftName = candidateName
        profileDraftRoleDeviceIDs = defaultProfileRoleDeviceIDs()
        profileDraftEnforceAudioDefaults = snapshot.activeSelection.enforceAudioDefaults
        render()
    }

    @objc private func beginEditProfile(_ sender: NSButton) {
        guard let profileID = profileActionProfileIDs[sender.tag],
              let profile = snapshot.profiles.first(where: { $0.id == profileID }) else {
            return
        }

        profileDraftID = profile.id
        profileDraftName = profile.name
        profileDraftRoleDeviceIDs = profile.selection.roleDeviceIDs
        profileDraftEnforceAudioDefaults = profile.selection.enforceAudioDefaults
        render()
    }

    @objc private func saveProfileDraft() {
        guard let profileID = profileDraftID else {
            return
        }

        let now = currentUnixMilliseconds()
        let existingProfile = snapshot.profiles.first { $0.id == profileID }
        let name = profileNameField?.stringValue.trimmingCharacters(in: .whitespacesAndNewlines) ?? profileDraftName
        profileDraftName = name.isEmpty ? "Untitled Profile" : name
        profileDraftEnforceAudioDefaults = profileEnforceButton?.state == .on

        for role in DeviceRole.allCases {
            guard let picker = profileRolePickers[role],
                  let deviceID = representedString(from: picker.selectedItem) else {
                continue
            }

            if deviceID.isEmpty {
                profileDraftRoleDeviceIDs.removeValue(forKey: role)
            } else {
                profileDraftRoleDeviceIDs[role] = deviceID
            }
        }

        let selection = SelectionViewModel(
            mode: .profile,
            profileID: profileID,
            roleDeviceIDs: profileDraftRoleDeviceIDs,
            enforceAudioDefaults: profileDraftEnforceAudioDefaults
        )
        let profile = ProfileViewModel(
            id: profileID,
            name: profileDraftName,
            selection: selection,
            createdAtUnixMilliseconds: existingProfile?.createdAtUnixMilliseconds ?? now,
            updatedAtUnixMilliseconds: now
        )

        clearProfileDraft()
        appState.saveProfile(profile)
        render()
    }

    @objc private func cancelProfileEditing() {
        clearProfileDraft()
        render()
    }

    @objc private func profileNameSubmitted(_ sender: NSTextField) {
        profileDraftName = sender.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    @objc private func profileEnforceChanged(_ sender: NSButton) {
        profileDraftEnforceAudioDefaults = sender.state == .on
    }

    @objc private func profilePickerChanged(_ sender: NSPopUpButton) {
        guard let role = profilePickerRoles[sender.tag],
              let deviceID = representedString(from: sender.selectedItem) else {
            return
        }

        if deviceID.isEmpty {
            profileDraftRoleDeviceIDs.removeValue(forKey: role)
        } else {
            profileDraftRoleDeviceIDs[role] = deviceID
        }
    }

    @objc private func activateProfileFromButton(_ sender: NSButton) {
        guard let profileID = profileActionProfileIDs[sender.tag] else {
            return
        }

        clearProfileDraft()
        appState.activateProfile(id: profileID)
    }

    @objc private func deleteProfileFromButton(_ sender: NSButton) {
        guard let profileID = profileActionProfileIDs[sender.tag] else {
            return
        }

        if profileDraftID == profileID {
            clearProfileDraft()
        }
        appState.deleteProfile(id: profileID)
    }

    private func resetProfileActionMaps() {
        profileActionProfileIDs.removeAll()
        profilePickerRoles.removeAll()
        nextProfileActionTag = 2000
    }

    private func clearProfileDraft() {
        profileDraftID = nil
        profileDraftName = ""
        profileDraftRoleDeviceIDs.removeAll()
        profileDraftEnforceAudioDefaults = true
        profileNameField = nil
        profileEnforceButton = nil
        profileRolePickers.removeAll()
    }

    private func defaultProfileRoleDeviceIDs() -> [DeviceRole: String] {
        var roleDeviceIDs: [DeviceRole: String] = [:]
        for role in DeviceRole.allCases {
            if let activeID = snapshot.activeSelection.deviceID(for: role) {
                roleDeviceIDs[role] = activeID
            } else if let currentID = currentDefaultDevice(for: role)?.id {
                roleDeviceIDs[role] = currentID
            }
        }
        return roleDeviceIDs
    }

    private func profileEditorTitle() -> String {
        guard let profileDraftID else {
            return "Profile"
        }

        if snapshot.profiles.contains(where: { $0.id == profileDraftID }) {
            return "Edit Profile"
        }

        return "Create Profile"
    }

    private func activeProfileDisplayName() -> String {
        guard snapshot.activeSelection.mode == .profile,
              let profileID = snapshot.activeSelection.profileID else {
            return "none"
        }

        return snapshot.profiles.first { $0.id == profileID }?.name ?? profileID
    }

    private func profileSummary(for profile: ProfileViewModel) -> String {
        let mode = profile.selection.enforceAudioDefaults ? "audio enforcement on" : "audio enforcement off"
        return "\(mode) / updated \(formatUnixMilliseconds(profile.updatedAtUnixMilliseconds))"
    }

    private func profileRoleSummary(for profile: ProfileViewModel) -> String {
        if profile.selection.roleDeviceIDs.isEmpty {
            return "No devices selected"
        }

        return DeviceRole.allCases.compactMap { role in
            guard let deviceID = profile.selection.deviceID(for: role) else {
                return nil
            }
            return "\(role.title): \(profileDeviceName(for: deviceID))"
        }.joined(separator: "\n")
    }

    private func profileDeviceName(for deviceID: String) -> String {
        guard let device = device(namedBy: deviceID) else {
            return "Missing: \(deviceID)"
        }

        return device.isConnected ? device.displayName : "\(device.displayName) (offline)"
    }

    private func currentUnixMilliseconds() -> UInt64 {
        UInt64(Date().timeIntervalSince1970 * 1000)
    }

    private func formatUnixMilliseconds(_ milliseconds: UInt64) -> String {
        let date = Date(timeIntervalSince1970: TimeInterval(milliseconds) / 1000)
        let formatter = DateFormatter()
        formatter.dateStyle = .short
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }

    private func makeDeviceRow(_ device: DeviceViewModel) -> NSView {
        let title = makeLabel(device.displayName, size: 13, weight: .medium, color: .labelColor)
        title.lineBreakMode = .byTruncatingMiddle

        let subtitle = makeLabel(deviceIdentitySummary(for: device), size: 12, weight: .regular, color: .secondaryLabelColor)
        subtitle.lineBreakMode = .byTruncatingMiddle

        let detail = makeLabel(deviceDetailSummary(for: device), size: 11, weight: .regular, color: .tertiaryLabelColor)
        detail.maximumNumberOfLines = 5
        detail.lineBreakMode = .byTruncatingTail

        let textStack = NSStackView(views: [title, subtitle, detail])
        textStack.orientation = .vertical
        textStack.alignment = .leading
        textStack.spacing = 3

        let statusStack = makeDeviceStatusStack(for: device)
        let row = NSStackView(views: [textStack, NSView(), statusStack])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 10, left: 12, bottom: 10, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        row.layer?.cornerRadius = 6
        return row
    }

    private func makeDeviceStatusStack(for device: DeviceViewModel) -> NSStackView {
        var labels = [makeStatusLabel(device.isConnected ? "Online" : "Offline")]
        if device.isDefaultInput {
            labels.append(makeStatusLabel("Default Input"))
        }
        if device.isDefaultOutput {
            labels.append(makeStatusLabel("Default Output"))
        }
        if device.isDefaultSystemOutput {
            labels.append(makeStatusLabel("System Output"))
        }

        let stack = NSStackView(views: labels)
        stack.orientation = .vertical
        stack.alignment = .trailing
        stack.spacing = 4
        return stack
    }

    private func makeDeviceCategoryHeader(
        category: DeviceCategory,
        visibleCount: Int,
        matchingCount: Int,
        hiddenOfflineCount: Int
    ) -> NSView {
        let isCollapsed = collapsedDeviceCategories.contains(category)
        let button = NSButton(title: category.title, target: self, action: #selector(toggleDeviceCategory(_:)))
        button.image = NSImage(
            systemSymbolName: isCollapsed ? "chevron.right" : "chevron.down",
            accessibilityDescription: isCollapsed ? "Expand" : "Collapse"
        )
        button.imagePosition = .imageLeading
        button.alignment = .left
        button.bezelStyle = .regularSquare
        button.isBordered = false
        button.font = NSFont.systemFont(ofSize: 15, weight: .semibold)
        button.contentTintColor = .labelColor
        button.toolTip = isCollapsed ? "Expand \(category.title)" : "Collapse \(category.title)"
        button.tag = nextDeviceCategoryTag
        deviceCategoryTags[button.tag] = category
        nextDeviceCategoryTag += 1

        var statusText = "\(visibleCount)"
        if hiddenOfflineCount > 0 {
            statusText = "\(visibleCount) shown / \(hiddenOfflineCount) hidden"
        } else if visibleCount != matchingCount {
            statusText = "\(visibleCount) shown / \(matchingCount) matching"
        }

        let countLabel = makeStatusLabel(statusText)
        let row = NSStackView(views: [button, NSView(), countLabel])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 10
        row.edgeInsets = NSEdgeInsets(top: 10, left: 0, bottom: 2, right: 0)
        return row
    }

    private func makeMetricRow(title: String, value: String, detail: String) -> NSView {
        let titleLabel = makeLabel(title, size: 13, weight: .medium, color: .secondaryLabelColor)
        let valueLabel = makeLabel(value, size: 14, weight: .semibold, color: .labelColor)
        valueLabel.lineBreakMode = .byTruncatingTail
        let detailLabel = makeLabel(detail, size: 12, weight: .regular, color: .tertiaryLabelColor)
        detailLabel.lineBreakMode = .byTruncatingMiddle

        let textStack = NSStackView(views: [valueLabel, detailLabel])
        textStack.orientation = .vertical
        textStack.alignment = .trailing
        textStack.spacing = 2

        let row = NSStackView(views: [titleLabel, NSView(), textStack])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = dynamicCGColor(.controlBackgroundColor)
        row.layer?.cornerRadius = 6
        return row
    }

    private func makeSectionHeader(title: String, count: Int) -> NSView {
        let titleLabel = makeLabel(title, size: 15, weight: .semibold, color: .labelColor)
        let countLabel = makeStatusLabel("\(count)")

        let row = NSStackView(views: [titleLabel, NSView(), countLabel])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 10
        row.edgeInsets = NSEdgeInsets(top: 10, left: 0, bottom: 2, right: 0)
        return row
    }

    private func makeStateView(title: String, detail: String, symbolName: String? = nil) -> NSView {
        var arrangedViews: [NSView] = []
        if let symbolName,
           let image = NSImage(systemSymbolName: symbolName, accessibilityDescription: title) {
            let imageView = NSImageView(image: image)
            imageView.symbolConfiguration = .init(pointSize: 30, weight: .regular)
            imageView.contentTintColor = .tertiaryLabelColor
            arrangedViews.append(imageView)
        }

        let titleLabel = makeLabel(title, size: 15, weight: .semibold, color: .labelColor)
        let detailLabel = makeLabel(detail, size: 13, weight: .regular, color: .secondaryLabelColor)
        detailLabel.isHidden = detail.isEmpty
        detailLabel.alignment = .center
        detailLabel.maximumNumberOfLines = 3

        arrangedViews.append(titleLabel)
        arrangedViews.append(detailLabel)

        let stack = NSStackView(views: arrangedViews)
        stack.orientation = .vertical
        stack.alignment = .centerX
        stack.spacing = 6
        stack.edgeInsets = NSEdgeInsets(top: 80, left: 16, bottom: 80, right: 16)
        return stack
    }

    private func makeMutedLabel(_ text: String) -> NSTextField {
        makeLabel(text, size: 12, weight: .regular, color: .tertiaryLabelColor)
    }

    private func makeStatusLabel(_ text: String) -> NSTextField {
        let label = makeLabel(text, size: 12, weight: .medium, color: .secondaryLabelColor)
        label.alignment = .center
        label.setContentHuggingPriority(.required, for: .horizontal)
        return label
    }

    private func makeLabel(
        _ text: String,
        size: CGFloat,
        weight: NSFont.Weight,
        color: NSColor
    ) -> NSTextField {
        let label = NSTextField(labelWithString: text)
        label.font = NSFont.systemFont(ofSize: size, weight: weight)
        label.textColor = color
        label.maximumNumberOfLines = 1
        label.lineBreakMode = .byTruncatingTail
        return label
    }

    private func categoriesVisibleInDeviceList() -> Set<DeviceCategory> {
        Set(filteredInventory().map(\.category))
    }

    private func searchedInventory() -> [DeviceViewModel] {
        let query = deviceFilterText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !query.isEmpty else {
            return snapshot.inventory
        }

        return snapshot.inventory.filter { device in
            searchableText(for: device).localizedCaseInsensitiveContains(query)
        }
    }

    private func filteredInventory() -> [DeviceViewModel] {
        let devices = searchedInventory()
        guard hideOfflineDevices else {
            return devices
        }

        return devices.filter(\.isConnected)
    }

    private func searchableText(for device: DeviceViewModel) -> String {
        var parts = [
            device.id,
            device.displayName,
            device.category.title,
            device.transport.title,
            device.connectionState.rawValue
        ]

        parts.append(contentsOf: [
            device.vendorName,
            device.modelName,
            device.serialNumber
        ].compactMap { $0 })
        parts.append(contentsOf: device.detailLines)

        return parts.joined(separator: " ")
    }

    private func deviceIdentitySummary(for device: DeviceViewModel) -> String {
        var parts = [
            device.transport.title,
            device.connectionState.rawValue
        ]

        if let maker = manufacturerModelSummary(for: device) {
            parts.append(maker)
        }

        if let serialNumber = device.serialNumber {
            parts.append("Serial: \(serialNumber)")
        }

        return parts.joined(separator: " / ")
    }

    private func manufacturerModelSummary(for device: DeviceViewModel) -> String? {
        let parts = [device.vendorName, device.modelName]
            .compactMap { value -> String? in
                guard let value, !value.isEmpty else {
                    return nil
                }
                return value
            }

        guard !parts.isEmpty else {
            return nil
        }

        return parts.joined(separator: " ")
    }

    private func deviceDetailSummary(for device: DeviceViewModel) -> String {
        var lines = ["ID: \(device.id)"]
        lines.append(contentsOf: defaultRoleLines(for: device))
        lines.append(contentsOf: device.detailLines.filter { detail in
            !detail.hasPrefix("Transport:") && !detail.hasPrefix("State:")
        })

        var seen = Set<String>()
        let uniqueLines = lines.filter { line in
            if seen.contains(line) {
                return false
            }
            seen.insert(line)
            return true
        }

        return uniqueLines.joined(separator: "\n")
    }

    private func defaultRoleLines(for device: DeviceViewModel) -> [String] {
        var lines: [String] = []
        if device.isDefaultInput {
            lines.append("Role: current default input")
        }
        if device.isDefaultOutput {
            lines.append("Role: current default output")
        }
        if device.isDefaultSystemOutput {
            lines.append("Role: current system output")
        }
        return lines
    }

    private func activeModeTitle() -> String {
        switch snapshot.activeSelection.mode {
        case .none:
            return "None"
        case .profile:
            return "Profile"
        case .manual:
            return "Manual Control"
        }
    }

    private func activeProfileName() -> String {
        guard snapshot.activeSelection.mode == .profile,
              let profileID = snapshot.activeSelection.profileID else {
            return snapshot.activeSelection.enforceAudioDefaults ? "Audio defaults enforced" : "Audio enforcement off"
        }

        return snapshot.profiles.first { $0.id == profileID }?.name ?? profileID
    }

    private func device(namedBy id: String) -> DeviceViewModel? {
        snapshot.inventory.first { $0.id == id }
    }

    private func currentDefaultDevice(for role: DeviceRole) -> DeviceViewModel? {
        switch role {
        case .defaultInput:
            return snapshot.inventory.first { $0.isDefaultInput }
        case .defaultOutput:
            return snapshot.inventory.first { $0.isDefaultOutput }
        case .systemOutput:
            return snapshot.inventory.first { $0.isDefaultSystemOutput }
        case .preferredCamera:
            guard let selectedID = snapshot.activeSelection.deviceID(for: role) else {
                return nil
            }
            return device(namedBy: selectedID)
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

        return snapshot.inventory
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

    private func currentRoleSummary(for role: DeviceRole) -> String {
        let selectedID = snapshot.activeSelection.deviceID(for: role)
        let selectedDevice = selectedID.flatMap(device(namedBy:))

        if let selectedDevice {
            return selectedDevice.isConnected
                ? "Selected: \(selectedDevice.displayName)"
                : "Selected offline: \(selectedDevice.displayName)"
        }

        if let selectedID {
            return "Missing: \(selectedID)"
        }

        if let currentDevice = currentDefaultDevice(for: role) {
            return "Current: \(currentDevice.displayName)"
        }

        return "Not selected"
    }

    private func selectedDeviceDetail(for deviceID: String) -> String {
        guard let device = device(namedBy: deviceID) else {
            return "Missing: \(deviceID)"
        }

        return deviceDetail(device)
    }

    private func deviceDetail(_ device: DeviceViewModel) -> String {
        let state = device.isConnected ? "Online" : "Offline"
        return "\(state) / \(device.subtitle)"
    }

    private func pickerTitle(for device: DeviceViewModel) -> String {
        device.isConnected ? device.displayName : "\(device.displayName) (offline)"
    }

    private func representedString(from item: NSMenuItem?) -> String? {
        if let value = item?.representedObject as? String {
            return value
        }
        if let value = item?.representedObject as? NSString {
            return value as String
        }
        return nil
    }

    private func tag(for role: DeviceRole) -> Int {
        switch role {
        case .defaultInput:
            return 1001
        case .defaultOutput:
            return 1002
        case .systemOutput:
            return 1003
        case .preferredCamera:
            return 1004
        }
    }

    private func role(for tag: Int) -> DeviceRole? {
        switch tag {
        case 1001:
            return .defaultInput
        case 1002:
            return .defaultOutput
        case 1003:
            return .systemOutput
        case 1004:
            return .preferredCamera
        default:
            return nil
        }
    }
}
