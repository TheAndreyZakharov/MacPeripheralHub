import Foundation

final class LoginItemController {
    private static let promptShownKey = "MacPeripheralHub.launchAtLoginPromptShown"
    private static let label = "com.theandreyzakharov.MacPeripheralHub.login"
    private static let launchAtLoginArgument = "--launch-at-login"
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
        LoginItemViewModel(status: launchAgentExists() ? .enabled : .notRegistered)
    }

    func setEnabled(_ enabled: Bool) throws -> LoginItemViewModel {
        if enabled {
            try installLaunchAgent()
        } else {
            try removeLaunchAgent()
        }

        return currentStatus()
    }

    private func installLaunchAgent() throws {
        let bundleURL = Bundle.main.bundleURL
        guard bundleURL.pathExtension == "app" else {
            throw LoginItemControllerError.missingApplicationBundle
        }

        let fileManager = FileManager.default
        let launchAgentsURL = try launchAgentsDirectory()
        try fileManager.createDirectory(at: launchAgentsURL, withIntermediateDirectories: true)

        let plist: [String: Any] = [
            "Label": Self.label,
            "ProgramArguments": [
                "/usr/bin/open",
                "-g",
                "-j",
                bundleURL.path,
                "--args",
                Self.launchAtLoginArgument
            ],
            "RunAtLoad": true,
            "KeepAlive": false,
            "LimitLoadToSessionType": "Aqua",
            "ProcessType": "Interactive"
        ]

        let data = try PropertyListSerialization.data(
            fromPropertyList: plist,
            format: .xml,
            options: 0
        )
        try data.write(to: launchAgentURL(), options: .atomic)
    }

    private func removeLaunchAgent() throws {
        let fileManager = FileManager.default
        let url = launchAgentURL()
        if fileManager.fileExists(atPath: url.path) {
            try fileManager.removeItem(at: url)
        }
    }

    private func launchAgentExists() -> Bool {
        FileManager.default.fileExists(atPath: launchAgentURL().path)
    }

    private func launchAgentURL() -> URL {
        let homeDirectory = FileManager.default.homeDirectoryForCurrentUser
        return homeDirectory
            .appendingPathComponent("Library", isDirectory: true)
            .appendingPathComponent("LaunchAgents", isDirectory: true)
            .appendingPathComponent("\(Self.label).plist")
    }

    private func launchAgentsDirectory() throws -> URL {
        guard let directory = try? FileManager.default.url(
            for: .libraryDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        ).appendingPathComponent("LaunchAgents", isDirectory: true) else {
            throw LoginItemControllerError.missingLaunchAgentsDirectory
        }

        return directory
    }
}

private enum LoginItemControllerError: LocalizedError {
    case missingApplicationBundle
    case missingLaunchAgentsDirectory

    var errorDescription: String? {
        switch self {
        case .missingApplicationBundle:
            return "MacPeripheralHub must be running from an app bundle to enable launch at login."
        case .missingLaunchAgentsDirectory:
            return "MacPeripheralHub could not find the user LaunchAgents directory."
        }
    }
}
