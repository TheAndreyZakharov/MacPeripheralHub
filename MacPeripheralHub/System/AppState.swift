import Combine
import Foundation

@MainActor
final class AppState: ObservableObject {
    @Published private(set) var inventory: [DeviceViewModel] = []
    @Published private(set) var profiles: [ProfileViewModel] = []
    @Published private(set) var activeSelection: SelectionViewModel = .empty
    @Published private(set) var isRefreshingInventory = false
    @Published var error: AppErrorViewModel?

    private let core: PeripheralCoreBridge

    init(core: PeripheralCoreBridge = PeripheralCoreBridge()) {
        self.core = core
    }

    func refreshAll() {
        refreshInventory()
        reloadProfiles()
        reloadActiveSelection()
    }

    func refreshInventory() {
        isRefreshingInventory = true
        let bridge = core

        Task {
            do {
                let devices = try await Task.detached(priority: .userInitiated) {
                    try bridge.loadInventory()
                }.value
                inventory = devices
                isRefreshingInventory = false
            } catch {
                isRefreshingInventory = false
                presentError(error, operation: "Refresh inventory")
            }
        }
    }

    func reloadProfiles() {
        let bridge = core

        Task {
            do {
                profiles = try await Task.detached(priority: .userInitiated) {
                    try bridge.loadProfiles()
                }.value
            } catch {
                presentError(error, operation: "Load profiles")
            }
        }
    }

    func reloadActiveSelection() {
        let bridge = core

        Task {
            do {
                activeSelection = try await Task.detached(priority: .userInitiated) {
                    try bridge.loadActiveSelection()
                }.value
            } catch {
                presentError(error, operation: "Load active selection")
            }
        }
    }

    func saveProfile(_ profile: ProfileViewModel) {
        let bridge = core

        Task {
            do {
                let savedProfile = try await Task.detached(priority: .userInitiated) {
                    try bridge.saveProfile(profile)
                }.value
                upsertProfile(savedProfile)
            } catch {
                presentError(error, operation: "Save profile")
            }
        }
    }

    func deleteProfile(id: String) {
        let bridge = core

        Task {
            do {
                try await Task.detached(priority: .userInitiated) {
                    try bridge.deleteProfile(id: id)
                }.value
                profiles.removeAll { $0.id == id }
                if activeSelection.profileID == id {
                    activeSelection = .empty
                }
            } catch {
                presentError(error, operation: "Delete profile")
            }
        }
    }

    func activateProfile(id: String) {
        let bridge = core

        Task {
            do {
                activeSelection = try await Task.detached(priority: .userInitiated) {
                    try bridge.activateProfile(id: id)
                }.value
                refreshInventory()
            } catch {
                presentError(error, operation: "Activate profile")
            }
        }
    }

    func activateProfile(_ profile: ProfileViewModel) {
        activateProfile(id: profile.id)
    }

    func setManualDevice(_ deviceID: String, for role: DeviceRole) {
        let bridge = core

        Task {
            do {
                activeSelection = try await Task.detached(priority: .userInitiated) {
                    try bridge.setManualDevice(deviceID, for: role)
                }.value
                refreshInventory()
            } catch {
                presentError(error, operation: "Apply manual selection")
            }
        }
    }

    func activateManualSelection(_ selection: SelectionViewModel) {
        let bridge = core

        Task {
            do {
                activeSelection = try await Task.detached(priority: .userInitiated) {
                    try bridge.activateManualSelection(selection)
                }.value
                refreshInventory()
            } catch {
                presentError(error, operation: "Activate manual selection")
            }
        }
    }

    func dismissError() {
        error = nil
    }

    private func upsertProfile(_ profile: ProfileViewModel) {
        if let index = profiles.firstIndex(where: { $0.id == profile.id }) {
            profiles[index] = profile
        } else {
            profiles.append(profile)
            profiles.sort { left, right in
                if left.name.localizedCaseInsensitiveCompare(right.name) != .orderedSame {
                    return left.name.localizedCaseInsensitiveCompare(right.name) == .orderedAscending
                }
                return left.id < right.id
            }
        }
    }

    private func presentError(_ error: Error, operation: String) {
        let message = error.localizedDescription.isEmpty ? String(describing: error) : error.localizedDescription
        self.error = AppErrorViewModel(operation: operation, message: message)
    }
}
