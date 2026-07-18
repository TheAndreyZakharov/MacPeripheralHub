import AppKit
import Combine

private enum AppSection: Int, CaseIterable {
    case manualControl
    case devices
    case profiles

    var title: String {
        switch self {
        case .manualControl:
            return "Manual Control"
        case .devices:
            return "Devices"
        case .profiles:
            return "Profiles"
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
        }
    }
}

final class RootView: NSView {
    private struct Snapshot {
        var inventory: [DeviceViewModel]
        var profiles: [ProfileViewModel]
        var activeSelection: SelectionViewModel
        var isRefreshingInventory: Bool
        var error: AppErrorViewModel?

        static let empty = Snapshot(
            inventory: [],
            profiles: [],
            activeSelection: .empty,
            isRefreshingInventory: false,
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
    private let contentStack = NSStackView()

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

    private func configureView() {
        wantsLayer = true
        layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor

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
        errorBanner.layer?.backgroundColor = NSColor.systemRed.withAlphaComponent(0.08).cgColor
        errorBanner.layer?.cornerRadius = 6
        errorBanner.isHidden = true

        errorLabel.font = NSFont.systemFont(ofSize: 13, weight: .regular)
        errorLabel.textColor = .systemRed
        errorLabel.maximumNumberOfLines = 2

        sidebarStack.orientation = .vertical
        sidebarStack.alignment = .width
        sidebarStack.spacing = 6
        sidebarStack.translatesAutoresizingMaskIntoConstraints = false

        contentStack.orientation = .vertical
        contentStack.alignment = .width
        contentStack.spacing = 10
        contentStack.edgeInsets = NSEdgeInsets(top: 16, left: 22, bottom: 24, right: 22)
    }

    private func configureLayout() {
        let rootStack = NSStackView()
        rootStack.orientation = .horizontal
        rootStack.alignment = .height
        rootStack.spacing = 0
        rootStack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(rootStack)

        sidebarView.wantsLayer = true
        sidebarView.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
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
        let documentView = NSView()
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
            contentStack.widthAnchor.constraint(equalTo: scrollView.contentView.widthAnchor)
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

    private func render() {
        titleLabel.stringValue = selectedSection.title
        subtitleLabel.stringValue = selectedSection.subtitle
        refreshButton.isEnabled = !snapshot.isRefreshingInventory

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
                ? NSColor.controlAccentColor.withAlphaComponent(0.12).cgColor
                : NSColor.clear.cgColor
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
            let dismissButton = NSButton()
            dismissButton.image = NSImage(systemSymbolName: "xmark", accessibilityDescription: "Dismiss")
            dismissButton.bezelStyle = .toolbar
            dismissButton.target = self
            dismissButton.action = #selector(dismissError)
            dismissButton.toolTip = "Dismiss error"

            let errorStack = NSStackView(views: [errorLabel, NSView(), dismissButton])
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
        }
    }

    private func makeManualViews() -> [NSView] {
        if snapshot.isRefreshingInventory && snapshot.inventory.isEmpty {
            return [makeStateView(title: "Loading devices", detail: "")]
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
        row.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
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

    private func makeDeviceViews() -> [NSView] {
        if snapshot.isRefreshingInventory && snapshot.inventory.isEmpty {
            return [makeStateView(title: "Loading devices", detail: "")]
        }

        if snapshot.inventory.isEmpty {
            return [makeStateView(title: "No devices found", detail: "")]
        }

        var views: [NSView] = []
        for category in DeviceCategory.allCases {
            let devices = snapshot.inventory.filter { $0.category == category }
            guard !devices.isEmpty else {
                continue
            }

            views.append(makeSectionHeader(title: category.title, count: devices.count))
            for device in devices.prefix(6) {
                views.append(makeDeviceRow(device))
            }
            if devices.count > 6 {
                views.append(makeMutedLabel("+\(devices.count - 6) more"))
            }
        }
        return views
    }

    private func makeProfileViews() -> [NSView] {
        if snapshot.profiles.isEmpty {
            return [makeStateView(title: "No profiles yet", detail: "")]
        }

        return snapshot.profiles.map { profile in
            let selected = snapshot.activeSelection.mode == .profile &&
                snapshot.activeSelection.profileID == profile.id
            let roleCount = profile.selection.roleDeviceIDs.count
            return makeMetricRow(
                title: profile.name,
                value: selected ? "Active" : "\(roleCount) selected",
                detail: profile.id
            )
        }
    }

    private func makeDeviceRow(_ device: DeviceViewModel) -> NSView {
        let title = makeLabel(device.displayName, size: 13, weight: .medium, color: .labelColor)
        let subtitle = makeLabel(device.subtitle, size: 12, weight: .regular, color: .secondaryLabelColor)
        let detail = makeLabel(device.detailLines.prefix(3).joined(separator: "  /  "), size: 11, weight: .regular, color: .tertiaryLabelColor)
        detail.maximumNumberOfLines = 2

        let textStack = NSStackView(views: [title, subtitle, detail])
        textStack.orientation = .vertical
        textStack.alignment = .leading
        textStack.spacing = 2

        let statusLabel = makeStatusLabel(device.isConnected ? "Online" : "Offline")
        let row = NSStackView(views: [textStack, NSView(), statusLabel])
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 9, left: 12, bottom: 9, right: 12)
        row.wantsLayer = true
        row.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        row.layer?.cornerRadius = 6
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
        row.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
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

    private func makeStateView(title: String, detail: String) -> NSView {
        let titleLabel = makeLabel(title, size: 15, weight: .semibold, color: .labelColor)
        let detailLabel = makeLabel(detail, size: 13, weight: .regular, color: .secondaryLabelColor)
        detailLabel.isHidden = detail.isEmpty

        let stack = NSStackView(views: [titleLabel, detailLabel])
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
