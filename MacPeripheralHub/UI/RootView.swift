import AppKit
import Combine

final class RootView: NSView {
    private let appState: AppState
    private var cancellables = Set<AnyCancellable>()

    private let titleLabel = NSTextField(labelWithString: "MacPeripheralHub")
    private let versionLabel = NSTextField(labelWithString: "")
    private let statusLabel = NSTextField(labelWithString: "")
    private let selectionLabel = NSTextField(labelWithString: "")
    private let errorLabel = NSTextField(labelWithString: "")

    init(appState: AppState, frame frameRect: NSRect = .zero) {
        self.appState = appState
        super.init(frame: frameRect)
        configureView()
        configureLayout()
        bindState()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("RootView does not support storyboard initialization.")
    }

    private func configureView() {
        wantsLayer = true
        layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor

        titleLabel.font = NSFont.systemFont(ofSize: 30, weight: .semibold)
        titleLabel.textColor = .labelColor

        versionLabel.font = NSFont.monospacedSystemFont(ofSize: 13, weight: .regular)
        versionLabel.textColor = .secondaryLabelColor

        statusLabel.font = NSFont.systemFont(ofSize: 15, weight: .regular)
        statusLabel.textColor = .secondaryLabelColor

        selectionLabel.font = NSFont.systemFont(ofSize: 14, weight: .regular)
        selectionLabel.textColor = .tertiaryLabelColor

        errorLabel.font = NSFont.systemFont(ofSize: 13, weight: .regular)
        errorLabel.textColor = .systemRed
        errorLabel.maximumNumberOfLines = 2
        errorLabel.isHidden = true
    }

    private func configureLayout() {
        let stackView = NSStackView(views: [titleLabel, versionLabel, statusLabel, selectionLabel, errorLabel])
        stackView.orientation = .vertical
        stackView.alignment = .leading
        stackView.spacing = 8
        stackView.translatesAutoresizingMaskIntoConstraints = false

        addSubview(stackView)

        NSLayoutConstraint.activate([
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 32),
            stackView.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -32),
            stackView.topAnchor.constraint(equalTo: topAnchor, constant: 32)
        ])
    }

    private func bindState() {
        appState.$inventory
            .combineLatest(appState.$profiles, appState.$activeSelection, appState.$isRefreshingInventory)
            .receive(on: RunLoop.main)
            .sink { [weak self] inventory, profiles, selection, isRefreshing in
                guard let self else {
                    return
                }

                versionLabel.stringValue = "Core bridge ready"
                let refreshState = isRefreshing ? "Refreshing" : "Ready"
                statusLabel.stringValue = "\(refreshState): \(inventory.count) devices, \(profiles.count) profiles"
                selectionLabel.stringValue = "Active mode: \(selection.mode.rawValue)"
            }
            .store(in: &cancellables)

        appState.$error
            .receive(on: RunLoop.main)
            .sink { [weak self] error in
                self?.errorLabel.isHidden = error == nil
                self?.errorLabel.stringValue = error.map { "\($0.operation): \($0.message)" } ?? ""
            }
            .store(in: &cancellables)
    }
}
