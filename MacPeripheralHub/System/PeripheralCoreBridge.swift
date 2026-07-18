import Foundation
import PeripheralCore

struct PeripheralCoreBridgeError: LocalizedError, Equatable {
    let operation: String
    let statusName: String

    var errorDescription: String? {
        "\(operation) failed: \(statusName)"
    }
}

final class PeripheralCoreBridge: @unchecked Sendable {
    func loadInventory() throws -> [DeviceViewModel] {
        let currentDevices = try withDeviceList(operation: "Create inventory list") { currentList in
            try withDatabase(operation: "Open application database") { db in
                try check(
                    mph_inventory_collect_and_store(db, currentList),
                    operation: "Refresh device inventory"
                )
                return mapDeviceList(currentList)
            }
        }

        let knownDevices = try loadKnownDevices()
        return mergeCurrentAndKnownDevices(currentDevices: currentDevices, knownDevices: knownDevices)
    }

    func loadKnownDevices() throws -> [DeviceViewModel] {
        try withDeviceList(operation: "Create known device list") { list in
            try withDatabase(operation: "Open application database") { db in
                try check(mph_db_load_known_devices(db, list), operation: "Load known devices")
                return mapDeviceList(list)
            }
        }
    }

    func loadProfiles() throws -> [ProfileViewModel] {
        try withProfileStore(operation: "Create profile store") { store in
            try withDatabase(operation: "Open application database") { db in
                try check(mph_db_load_profiles(db, store), operation: "Load profiles")
                return mapProfileStore(store)
            }
        }
    }

    func loadActiveSelection() throws -> SelectionViewModel {
        try withDatabase(operation: "Open application database") { db in
            var selection = mph_selection_t()
            mph_selection_init(&selection)
            var found = false
            try check(
                mph_db_load_active_selection(db, &selection, &found),
                operation: "Load active selection"
            )
            return found ? mapSelection(&selection) : .empty
        }
    }

    func saveProfile(_ profile: ProfileViewModel) throws -> ProfileViewModel {
        var cProfile = try makeProfile(profile)
        try withDatabase(operation: "Open application database") { db in
            try check(mph_db_save_profile(db, &cProfile), operation: "Save profile")
        }
        return mapProfile(&cProfile)
    }

    func deleteProfile(id: String) throws {
        try withDatabase(operation: "Open application database") { db in
            var activeSelection = mph_selection_t()
            mph_selection_init(&activeSelection)
            var foundActiveSelection = false
            try check(
                mph_db_load_active_selection(db, &activeSelection, &foundActiveSelection),
                operation: "Load active selection before profile delete"
            )

            try id.withCString { profileID in
                try check(mph_db_delete_profile(db, profileID), operation: "Delete profile")
            }

            let activeProfileID = cString(mph_swift_selection_profile_id_cstr(&activeSelection))
            if foundActiveSelection,
               activeSelection.mode == MPH_ACTIVE_MODE_PROFILE,
               activeProfileID == id {
                var emptySelection = mph_selection_t()
                mph_selection_init(&emptySelection)
                try check(
                    mph_db_save_active_selection(db, &emptySelection),
                    operation: "Clear deleted active profile"
                )
            }
        }
    }

    func activateProfile(id: String) throws -> SelectionViewModel {
        try withDatabase(operation: "Open application database") { db in
            var profile = mph_profile_t()
            mph_profile_init(&profile)
            var found = false
            try id.withCString { profileID in
                try check(
                    mph_db_load_profile(db, profileID, &profile, &found),
                    operation: "Load profile for activation"
                )
            }
            guard found else {
                throw makeError(status: MPH_STATUS_NOT_FOUND, operation: "Activate profile")
            }

            var selection = profile.selection
            selection.mode = MPH_ACTIVE_MODE_PROFILE
            try id.withCString { profileID in
                try check(
                    mph_selection_set_profile_id(&selection, profileID),
                    operation: "Mark active profile"
                )
            }
            try check(
                mph_db_save_active_selection(db, &selection),
                operation: "Save active profile selection"
            )
            try applyAudioDefaults(from: &selection, operation: "Apply profile audio defaults")
            return mapSelection(&selection)
        }
    }

    func activateManualSelection(_ selectionModel: SelectionViewModel) throws -> SelectionViewModel {
        var selection = try makeSelection(selectionModel, mode: .manual, profileID: nil)
        try withDatabase(operation: "Open application database") { db in
            try check(
                mph_db_save_active_selection(db, &selection),
                operation: "Save manual selection"
            )
        }
        try applyAudioDefaults(from: &selection, operation: "Apply manual audio defaults")
        return mapSelection(&selection)
    }

    func setManualDevice(_ deviceID: String, for role: DeviceRole) throws -> SelectionViewModel {
        var activeSelection = try loadActiveSelection()
        if activeSelection.mode != .manual {
            activeSelection = .empty
        }

        activeSelection.mode = .manual
        activeSelection.profileID = nil
        activeSelection.roleDeviceIDs[role] = deviceID
        return try activateManualSelection(activeSelection)
    }

    func makeAudioWatcher(
        desiredSelection: SelectionViewModel,
        eventHandler: @escaping @Sendable (AudioWatcherEventViewModel) -> Void
    ) throws -> PeripheralAudioWatcher {
        let selection = try makeSelection(
            desiredSelection,
            mode: desiredSelection.mode,
            profileID: desiredSelection.profileID
        )
        return try PeripheralAudioWatcher(
            initialSelection: selection,
            eventHandler: eventHandler
        )
    }

    func updateAudioWatcher(
        _ watcher: PeripheralAudioWatcher,
        desiredSelection: SelectionViewModel
    ) throws {
        let selection = try makeSelection(
            desiredSelection,
            mode: desiredSelection.mode,
            profileID: desiredSelection.profileID
        )
        try watcher.setDesiredSelection(selection)
    }

    private func withDatabase<T>(operation: String, _ body: (OpaquePointer) throws -> T) throws -> T {
        var db: OpaquePointer?
        try check(mph_db_open_application_support(&db), operation: operation)
        guard let db else {
            throw makeError(status: MPH_STATUS_INTERNAL_ERROR, operation: operation)
        }
        defer { mph_db_close(db) }

        try check(mph_db_migrate(db), operation: "Migrate application database")
        return try body(db)
    }

    private func withDeviceList<T>(operation: String, _ body: (OpaquePointer) throws -> T) throws -> T {
        guard let list = mph_device_list_create() else {
            throw makeError(status: MPH_STATUS_NO_MEMORY, operation: operation)
        }
        defer { mph_device_list_destroy(list) }
        return try body(list)
    }

    private func withProfileStore<T>(
        operation: String,
        _ body: (OpaquePointer) throws -> T
    ) throws -> T {
        guard let store = mph_profile_store_create() else {
            throw makeError(status: MPH_STATUS_NO_MEMORY, operation: operation)
        }
        defer { mph_profile_store_destroy(store) }
        return try body(store)
    }

    private func check(_ status: mph_status_t, operation: String) throws {
        guard mph_status_is_ok(status) else {
            throw makeError(status: status, operation: operation)
        }
    }

    private func makeError(status: mph_status_t, operation: String) -> PeripheralCoreBridgeError {
        PeripheralCoreBridgeError(
            operation: operation,
            statusName: cString(mph_status_name(status))
        )
    }

    private func mapDeviceList(_ list: OpaquePointer) -> [DeviceViewModel] {
        let count = mph_device_list_count(list)
        var devices: [DeviceViewModel] = []
        devices.reserveCapacity(count)

        for index in 0..<count {
            guard let device = mph_device_list_get(list, index) else {
                continue
            }
            devices.append(mapDevice(device))
        }

        return devices
    }

    private func mapDevice(_ devicePointer: UnsafePointer<mph_device_t>) -> DeviceViewModel {
        let device = devicePointer.pointee
        let category = DeviceCategory(cValue: device.category)
        let transport = DeviceTransport(cValue: device.transport)
        let connectionState = DeviceConnectionState(cValue: device.connection_state)
        let name = cString(mph_swift_device_display_name_cstr(devicePointer))
        let fallbackName = cString(mph_swift_device_id_cstr(devicePointer))

        return DeviceViewModel(
            id: fallbackName,
            displayName: name.isEmpty ? fallbackName : name,
            vendorName: optionalString(mph_swift_device_vendor_name_cstr(devicePointer)),
            modelName: optionalString(mph_swift_device_model_name_cstr(devicePointer)),
            serialNumber: optionalString(mph_swift_device_serial_number_cstr(devicePointer)),
            category: category,
            transport: transport,
            connectionState: connectionState,
            isConnected: device.is_connected,
            isDefaultInput: device.audio.is_default_input,
            isDefaultOutput: device.audio.is_default_output,
            isDefaultSystemOutput: device.audio.is_default_system_output,
            detailLines: makeDetailLines(for: device, pointer: devicePointer)
        )
    }

    private func makeDetailLines(
        for device: mph_device_t,
        pointer: UnsafePointer<mph_device_t>
    ) -> [String] {
        var details: [String] = []
        details.append("Transport: \(DeviceTransport(cValue: device.transport).title)")
        details.append("State: \(DeviceConnectionState(cValue: device.connection_state).rawValue)")

        if device.display.width_px > 0 && device.display.height_px > 0 {
            var displayLine = "Resolution: \(device.display.width_px)x\(device.display.height_px)"
            if device.display.refresh_rate_hz > 0 {
                displayLine += " @ \(formatHertz(device.display.refresh_rate_hz))"
            }
            if device.display.is_main {
                displayLine += " / main"
            }
            details.append(displayLine)
        }

        if device.audio.sample_rate_hz > 0 || device.audio.channel_count > 0 {
            var audioParts: [String] = []
            if device.audio.sample_rate_hz > 0 {
                audioParts.append("Sample rate: \(formatHertz(device.audio.sample_rate_hz))")
            }
            if device.audio.channel_count > 0 {
                audioParts.append("Channels: \(device.audio.channel_count)")
            }
            details.append(audioParts.joined(separator: " / "))
        }

        if let cameraUID = optionalString(mph_swift_device_camera_unique_id_cstr(pointer)) {
            details.append("Camera UID: \(cameraUID)")
            details.append(
                "Global default camera: \(device.camera.supports_global_default ? "supported" : "not available")"
            )
        }

        if device.hid.vendor_id > 0 || device.hid.product_id > 0 {
            details.append(
                "HID: vendor \(hex(device.hid.vendor_id)) / product \(hex(device.hid.product_id))"
            )
        }

        if device.hid.usage_page > 0 || device.hid.usage > 0 {
            details.append("Usage: page \(device.hid.usage_page) / usage \(device.hid.usage)")
        }

        if device.usb.vendor_id > 0 || device.usb.product_id > 0 {
            details.append(
                "USB: vendor \(hex(device.usb.vendor_id)) / product \(hex(device.usb.product_id))"
            )
        }

        if device.usb.speed_mbps > 0 || device.usb.power_ma > 0 {
            details.append("USB link: \(device.usb.speed_mbps) Mbps / \(device.usb.power_ma) mA")
        }

        if let bluetoothAddress = optionalString(mph_swift_device_bluetooth_address_cstr(pointer)) {
            let pairing = device.bluetooth.is_paired ? "paired" : "not paired"
            let connection = device.bluetooth.is_connected ? "connected" : "disconnected"
            details.append("Bluetooth: \(bluetoothAddress) / \(pairing) / \(connection)")
        }

        if device.audio.is_default_input {
            details.append("Current default input")
        }
        if device.audio.is_default_output {
            details.append("Current default output")
        }
        if device.audio.is_default_system_output {
            details.append("Current system output")
        }

        return details
    }

    private func mapProfileStore(_ store: OpaquePointer) -> [ProfileViewModel] {
        let count = mph_profile_store_count(store)
        var profiles: [ProfileViewModel] = []
        profiles.reserveCapacity(count)

        for index in 0..<count {
            guard let profile = mph_profile_store_get(store, index) else {
                continue
            }
            profiles.append(mapProfile(profile))
        }

        return profiles
    }

    private func mapProfile(_ profilePointer: UnsafePointer<mph_profile_t>) -> ProfileViewModel {
        let profile = profilePointer.pointee
        var selection = profile.selection
        return ProfileViewModel(
            id: cString(mph_swift_profile_id_cstr(profilePointer)),
            name: cString(mph_swift_profile_name_cstr(profilePointer)),
            selection: mapSelection(&selection),
            createdAtUnixMilliseconds: UInt64(profile.created_at_unix_ms),
            updatedAtUnixMilliseconds: UInt64(profile.updated_at_unix_ms)
        )
    }

    private func mapProfile(_ profile: inout mph_profile_t) -> ProfileViewModel {
        withUnsafePointer(to: &profile) { pointer in
            mapProfile(pointer)
        }
    }

    private func mapSelection(_ selection: inout mph_selection_t) -> SelectionViewModel {
        var roleDeviceIDs: [DeviceRole: String] = [:]
        for role in DeviceRole.allCases {
            let deviceID = cString(
                mph_swift_selection_role_device_id_cstr(&selection, role.cValue)
            )
            if !deviceID.isEmpty {
                roleDeviceIDs[role] = deviceID
            }
        }

        let profileID = optionalString(mph_swift_selection_profile_id_cstr(&selection))
        return SelectionViewModel(
            mode: ActiveMode(cValue: selection.mode),
            profileID: profileID,
            roleDeviceIDs: roleDeviceIDs,
            enforceAudioDefaults: selection.enforce_audio_defaults
        )
    }

    private func makeProfile(_ profile: ProfileViewModel) throws -> mph_profile_t {
        var cProfile = mph_profile_t()
        mph_profile_init(&cProfile)

        try profile.id.withCString { profileID in
            try profile.name.withCString { profileName in
                try check(
                    mph_profile_configure(&cProfile, profileID, profileName),
                    operation: "Configure profile"
                )
            }
        }

        let selection = try makeSelection(profile.selection, mode: .profile, profileID: profile.id)
        cProfile.selection = selection
        cProfile.created_at_unix_ms = profile.createdAtUnixMilliseconds
        cProfile.updated_at_unix_ms = profile.updatedAtUnixMilliseconds
        return cProfile
    }

    private func makeSelection(
        _ selectionModel: SelectionViewModel,
        mode: ActiveMode,
        profileID: String?
    ) throws -> mph_selection_t {
        var selection = mph_selection_t()
        mph_selection_init(&selection)
        selection.mode = mode.cValue
        selection.enforce_audio_defaults = selectionModel.enforceAudioDefaults

        if let profileID {
            try profileID.withCString { cProfileID in
                try check(
                    mph_selection_set_profile_id(&selection, cProfileID),
                    operation: "Set selection profile id"
                )
            }
        }

        for (role, deviceID) in selectionModel.roleDeviceIDs where !deviceID.isEmpty {
            var cDeviceID = mph_device_id_t()
            mph_device_id_clear(&cDeviceID)
            try deviceID.withCString { value in
                try check(
                    mph_device_id_set(&cDeviceID, value),
                    operation: "Set selected device id"
                )
            }
            try check(
                mph_selection_set_role_device(&selection, role.cValue, &cDeviceID),
                operation: "Set selected device role"
            )
        }

        return selection
    }

    private func applyAudioDefaults(from selection: inout mph_selection_t, operation: String) throws {
        guard selection.enforce_audio_defaults else {
            return
        }

        try applyAudioRole(.defaultInput, from: &selection, operation: operation)
        try applyAudioRole(.defaultOutput, from: &selection, operation: operation)
        try applyAudioRole(.systemOutput, from: &selection, operation: operation)
    }

    private func applyAudioRole(
        _ role: DeviceRole,
        from selection: inout mph_selection_t,
        operation: String
    ) throws {
        guard role.isSystemAudioRole,
              let deviceID = mph_selection_get_role_device(&selection, role.cValue),
              !mph_device_id_is_empty(deviceID) else {
            return
        }

        let status: mph_status_t
        switch role {
        case .defaultInput:
            status = mph_core_audio_set_default_input(deviceID)
        case .defaultOutput:
            status = mph_core_audio_set_default_output(deviceID)
        case .systemOutput:
            status = mph_core_audio_set_default_system_output(deviceID)
        case .preferredCamera:
            status = MPH_STATUS_OK
        }

        try check(status, operation: "\(operation): \(role.title)")
    }

    private func mergeCurrentAndKnownDevices(
        currentDevices: [DeviceViewModel],
        knownDevices: [DeviceViewModel]
    ) -> [DeviceViewModel] {
        var mergedByID: [String: DeviceViewModel] = [:]
        for device in knownDevices {
            mergedByID[device.id] = device
        }
        for device in currentDevices {
            mergedByID[device.id] = device
        }

        return mergedByID.values.sorted { left, right in
            if left.category.rawValue != right.category.rawValue {
                let leftIndex = DeviceCategory.allCases.firstIndex(of: left.category) ?? 0
                let rightIndex = DeviceCategory.allCases.firstIndex(of: right.category) ?? 0
                return leftIndex < rightIndex
            }
            if left.displayName.localizedCaseInsensitiveCompare(right.displayName) != .orderedSame {
                return left.displayName.localizedCaseInsensitiveCompare(right.displayName) == .orderedAscending
            }
            return left.id < right.id
        }
    }

    private func optionalString(_ value: UnsafePointer<CChar>?) -> String? {
        let string = cString(value)
        return string.isEmpty ? nil : string
    }

    private func cString(_ value: UnsafePointer<CChar>?) -> String {
        guard let value else {
            return ""
        }
        return String(cString: value)
    }

    private func formatHertz(_ value: Double) -> String {
        value.rounded() == value ? "\(Int(value)) Hz" : String(format: "%.2f Hz", value)
    }

    private func hex(_ value: UInt32) -> String {
        value == 0 ? "0" : String(format: "0x%04X", value)
    }
}

private final class AudioWatcherCallbackBox: @unchecked Sendable {
    let handler: @Sendable (AudioWatcherEventViewModel) -> Void

    init(handler: @escaping @Sendable (AudioWatcherEventViewModel) -> Void) {
        self.handler = handler
    }
}

private func audioWatcherCallback(
    _ eventPointer: UnsafePointer<mph_audio_watcher_event_t>?,
    _ context: UnsafeMutableRawPointer?
) {
    guard let eventPointer, let context else {
        return
    }

    let box = Unmanaged<AudioWatcherCallbackBox>
        .fromOpaque(context)
        .takeUnretainedValue()
    let event = eventPointer.pointee
    box.handler(
        AudioWatcherEventViewModel(
            flags: event.flags,
            statusName: String(cString: mph_status_name(event.status)),
            actionCount: event.plan.action_count,
            availableAudioDeviceCount: event.available_audio_device_count,
            timestampUnixMilliseconds: event.timestamp_unix_ms
        )
    )
}

final class PeripheralAudioWatcher: @unchecked Sendable {
    private var watcher: OpaquePointer?
    private let callbackBox: AudioWatcherCallbackBox

    init(
        initialSelection: mph_selection_t,
        eventHandler: @escaping @Sendable (AudioWatcherEventViewModel) -> Void
    ) throws {
        callbackBox = AudioWatcherCallbackBox(handler: eventHandler)

        var config = mph_audio_watcher_config_t()
        mph_audio_watcher_config_init(&config)
        config.desired_selection = initialSelection
        config.apply_reconcile_actions = true
        config.callback = audioWatcherCallback
        config.callback_context = Unmanaged.passUnretained(callbackBox).toOpaque()

        var createdWatcher: OpaquePointer?
        let status = mph_audio_watcher_create(&createdWatcher, &config)
        guard mph_status_is_ok(status), let createdWatcher else {
            throw PeripheralCoreBridgeError(
                operation: "Create background audio watcher",
                statusName: String(cString: mph_status_name(status))
            )
        }

        watcher = createdWatcher
    }

    deinit {
        stop()
        if let watcher {
            mph_audio_watcher_destroy(watcher)
        }
    }

    func start() throws {
        try withWatcher(operation: "Start background audio watcher") { watcher in
            mph_audio_watcher_start(watcher)
        }
    }

    func stop() {
        if let watcher {
            mph_audio_watcher_stop(watcher)
        }
    }

    func setDesiredSelection(_ selection: mph_selection_t) throws {
        try withWatcher(operation: "Update background audio selection") { watcher in
            var mutableSelection = selection
            return withUnsafePointer(to: &mutableSelection) { pointer in
                mph_audio_watcher_set_desired_selection(watcher, pointer)
            }
        }
    }

    func requestManualScan() throws {
        try withWatcher(operation: "Request background audio scan") { watcher in
            mph_audio_watcher_request_scan(watcher, UInt32(MPH_AUDIO_WATCHER_EVENT_MANUAL_SCAN.rawValue))
        }
    }

    var isRunning: Bool {
        guard let watcher else {
            return false
        }
        return mph_audio_watcher_is_running(watcher)
    }

    private func withWatcher(
        operation: String,
        _ body: (OpaquePointer) -> mph_status_t
    ) throws {
        guard let watcher else {
            throw PeripheralCoreBridgeError(
                operation: operation,
                statusName: String(cString: mph_status_name(MPH_STATUS_INVALID_ARGUMENT))
            )
        }

        let status = body(watcher)
        guard mph_status_is_ok(status) else {
            throw PeripheralCoreBridgeError(
                operation: operation,
                statusName: String(cString: mph_status_name(status))
            )
        }
    }
}
