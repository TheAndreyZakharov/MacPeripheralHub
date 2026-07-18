import AppKit
import PeripheralCore

final class RootView: NSView {
    private let titleLabel = NSTextField(labelWithString: "MacPeripheralHub")
    private let versionLabel = NSTextField(labelWithString: "Core \(String(cString: mph_core_version()))")
    private let statusLabel = NSTextField(labelWithString: "Application shell is ready.")

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        configureView()
        configureLayout()
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
        statusLabel.textColor = .tertiaryLabelColor
    }

    private func configureLayout() {
        let stackView = NSStackView(views: [titleLabel, versionLabel, statusLabel])
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
}
