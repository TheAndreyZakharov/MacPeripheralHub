import Foundation
import ServiceManagement

final class LoginItemController {
    private static let promptShownKey = "MacPeripheralHub.launchAtLoginPromptShown"
    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    var hasShownPrompt: Bool {
        defaults.bool(forKey: Self.promptShownKey)
    }

    func markPromptShown() {
        defaults.set(true, forKey: Self.promptShownKey)
    }

    func currentStatus() -> LoginItemViewModel {
        LoginItemViewModel(status: mapStatus(SMAppService.mainApp.status))
    }

    func setEnabled(_ enabled: Bool) throws -> LoginItemViewModel {
        let service = SMAppService.mainApp

        if enabled {
            if service.status != .enabled {
                try service.register()
            }
        } else if service.status != .notRegistered {
            try service.unregister()
        }

        return currentStatus()
    }

    private func mapStatus(_ status: SMAppService.Status) -> LoginItemStatus {
        switch status {
        case .notRegistered:
            return .notRegistered
        case .enabled:
            return .enabled
        case .requiresApproval:
            return .requiresApproval
        case .notFound:
            return .notFound
        @unknown default:
            return .notFound
        }
    }
}
