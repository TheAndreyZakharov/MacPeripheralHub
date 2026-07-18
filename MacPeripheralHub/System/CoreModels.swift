import Foundation
import PeripheralCore

enum DeviceCategory: String, CaseIterable, Identifiable, Sendable {
    case display
    case audioInput = "audio_input"
    case audioOutput = "audio_output"
    case audioSystemOutput = "audio_system_output"
    case camera
    case keyboard
    case mouse
    case trackpad
    case bluetooth
    case usb
    case hub
    case dock
    case audioInterface = "audio_interface"
    case unknown

    var id: String { rawValue }

    var title: String {
        switch self {
        case .display:
            return "Displays"
        case .audioInput:
            return "Microphones"
        case .audioOutput:
            return "Audio Outputs"
        case .audioSystemOutput:
            return "System Audio Outputs"
        case .camera:
            return "Cameras"
        case .keyboard:
            return "Keyboards"
        case .mouse:
            return "Mice"
        case .trackpad:
            return "Trackpads"
        case .bluetooth:
            return "Bluetooth"
        case .usb:
            return "USB"
        case .hub:
            return "Hubs"
        case .dock:
            return "Docks"
        case .audioInterface:
            return "Audio Interfaces"
        case .unknown:
            return "Unknown"
        }
    }

    init(cValue: mph_device_category_t) {
        switch cValue {
        case MPH_DEVICE_CATEGORY_DISPLAY:
            self = .display
        case MPH_DEVICE_CATEGORY_AUDIO_INPUT:
            self = .audioInput
        case MPH_DEVICE_CATEGORY_AUDIO_OUTPUT:
            self = .audioOutput
        case MPH_DEVICE_CATEGORY_AUDIO_SYSTEM_OUTPUT:
            self = .audioSystemOutput
        case MPH_DEVICE_CATEGORY_CAMERA:
            self = .camera
        case MPH_DEVICE_CATEGORY_KEYBOARD:
            self = .keyboard
        case MPH_DEVICE_CATEGORY_MOUSE:
            self = .mouse
        case MPH_DEVICE_CATEGORY_TRACKPAD:
            self = .trackpad
        case MPH_DEVICE_CATEGORY_BLUETOOTH:
            self = .bluetooth
        case MPH_DEVICE_CATEGORY_USB:
            self = .usb
        case MPH_DEVICE_CATEGORY_HUB:
            self = .hub
        case MPH_DEVICE_CATEGORY_DOCK:
            self = .dock
        case MPH_DEVICE_CATEGORY_AUDIO_INTERFACE:
            self = .audioInterface
        default:
            self = .unknown
        }
    }
}

enum DeviceTransport: String, CaseIterable, Identifiable, Sendable {
    case builtIn = "built_in"
    case usb
    case bluetooth
    case thunderbolt
    case hdmi
    case displayPort = "display_port"
    case virtual
    case aggregate
    case unknown

    var id: String { rawValue }

    var title: String {
        switch self {
        case .builtIn:
            return "Built-in"
        case .usb:
            return "USB"
        case .bluetooth:
            return "Bluetooth"
        case .thunderbolt:
            return "Thunderbolt"
        case .hdmi:
            return "HDMI"
        case .displayPort:
            return "DisplayPort"
        case .virtual:
            return "Virtual"
        case .aggregate:
            return "Aggregate"
        case .unknown:
            return "Unknown"
        }
    }

    init(cValue: mph_device_transport_t) {
        switch cValue {
        case MPH_DEVICE_TRANSPORT_BUILT_IN:
            self = .builtIn
        case MPH_DEVICE_TRANSPORT_USB:
            self = .usb
        case MPH_DEVICE_TRANSPORT_BLUETOOTH:
            self = .bluetooth
        case MPH_DEVICE_TRANSPORT_THUNDERBOLT:
            self = .thunderbolt
        case MPH_DEVICE_TRANSPORT_HDMI:
            self = .hdmi
        case MPH_DEVICE_TRANSPORT_DISPLAY_PORT:
            self = .displayPort
        case MPH_DEVICE_TRANSPORT_VIRTUAL:
            self = .virtual
        case MPH_DEVICE_TRANSPORT_AGGREGATE:
            self = .aggregate
        default:
            self = .unknown
        }
    }
}

enum DeviceConnectionState: String, CaseIterable, Identifiable, Sendable {
    case unknown
    case connected
    case disconnected
    case unavailable

    var id: String { rawValue }

    init(cValue: mph_device_connection_state_t) {
        switch cValue {
        case MPH_DEVICE_CONNECTION_CONNECTED:
            self = .connected
        case MPH_DEVICE_CONNECTION_DISCONNECTED:
            self = .disconnected
        case MPH_DEVICE_CONNECTION_UNAVAILABLE:
            self = .unavailable
        default:
            self = .unknown
        }
    }
}

enum ActiveMode: String, CaseIterable, Identifiable, Sendable {
    case none
    case profile
    case manual

    var id: String { rawValue }

    init(cValue: mph_active_mode_t) {
        switch cValue {
        case MPH_ACTIVE_MODE_PROFILE:
            self = .profile
        case MPH_ACTIVE_MODE_MANUAL:
            self = .manual
        default:
            self = .none
        }
    }

    var cValue: mph_active_mode_t {
        switch self {
        case .none:
            return MPH_ACTIVE_MODE_NONE
        case .profile:
            return MPH_ACTIVE_MODE_PROFILE
        case .manual:
            return MPH_ACTIVE_MODE_MANUAL
        }
    }
}

enum DeviceRole: String, CaseIterable, Identifiable, Sendable {
    case defaultInput = "default_input"
    case defaultOutput = "default_output"
    case systemOutput = "system_output"
    case preferredCamera = "preferred_camera"

    var id: String { rawValue }

    var title: String {
        switch self {
        case .defaultInput:
            return "Default Input"
        case .defaultOutput:
            return "Default Output"
        case .systemOutput:
            return "System Output"
        case .preferredCamera:
            return "Preferred Camera"
        }
    }

    var cValue: mph_device_role_t {
        switch self {
        case .defaultInput:
            return MPH_DEVICE_ROLE_DEFAULT_INPUT
        case .defaultOutput:
            return MPH_DEVICE_ROLE_DEFAULT_OUTPUT
        case .systemOutput:
            return MPH_DEVICE_ROLE_SYSTEM_OUTPUT
        case .preferredCamera:
            return MPH_DEVICE_ROLE_PREFERRED_CAMERA
        }
    }

    var isSystemAudioRole: Bool {
        self == .defaultInput || self == .defaultOutput || self == .systemOutput
    }
}

struct DeviceViewModel: Identifiable, Equatable, Sendable {
    let id: String
    let displayName: String
    let vendorName: String?
    let modelName: String?
    let serialNumber: String?
    let category: DeviceCategory
    let transport: DeviceTransport
    let connectionState: DeviceConnectionState
    let isConnected: Bool
    let isDefaultInput: Bool
    let isDefaultOutput: Bool
    let isDefaultSystemOutput: Bool
    let detailLines: [String]

    var subtitle: String {
        [transport.title, connectionState.rawValue].joined(separator: " / ")
    }
}

struct SelectionViewModel: Equatable, Sendable {
    var mode: ActiveMode
    var profileID: String?
    var roleDeviceIDs: [DeviceRole: String]
    var enforceAudioDefaults: Bool

    static let empty = SelectionViewModel(
        mode: .none,
        profileID: nil,
        roleDeviceIDs: [:],
        enforceAudioDefaults: true
    )

    func deviceID(for role: DeviceRole) -> String? {
        roleDeviceIDs[role]
    }
}

struct ProfileViewModel: Identifiable, Equatable, Sendable {
    let id: String
    var name: String
    var selection: SelectionViewModel
    var createdAtUnixMilliseconds: UInt64
    var updatedAtUnixMilliseconds: UInt64
}

enum LoginItemStatus: String, Equatable, Sendable {
    case notRegistered
    case enabled
    case requiresApproval
    case notFound

    var title: String {
        switch self {
        case .notRegistered:
            return "Off"
        case .enabled:
            return "On"
        case .requiresApproval:
            return "Needs Approval"
        case .notFound:
            return "Unavailable"
        }
    }

    var detail: String {
        switch self {
        case .notRegistered:
            return "MacPeripheralHub will not start automatically when you log in."
        case .enabled:
            return "MacPeripheralHub will start automatically when you log in."
        case .requiresApproval:
            return "Allow MacPeripheralHub in System Settings to start at login."
        case .notFound:
            return "macOS could not find this app as a login item."
        }
    }
}

struct LoginItemViewModel: Equatable, Sendable {
    let status: LoginItemStatus

    static let unknown = LoginItemViewModel(status: .notRegistered)

    var isEnabled: Bool {
        status == .enabled
    }

    var canToggle: Bool {
        status != .notFound
    }
}

struct AppErrorViewModel: Identifiable, Equatable, Sendable {
    let id = UUID()
    let operation: String
    let message: String
}
