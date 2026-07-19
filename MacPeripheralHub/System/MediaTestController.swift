import AppKit
@preconcurrency import AVFoundation

enum MediaTestError: LocalizedError {
    case glassSoundUnavailable
    case microphonePermissionDenied
    case microphoneUnavailable
    case cameraPermissionDenied
    case cameraUnavailable

    var errorDescription: String? {
        switch self {
        case .glassSoundUnavailable:
            return "The built-in Glass system sound could not be loaded."
        case .microphonePermissionDenied:
            return "Microphone access is not allowed for MacPeripheralHub."
        case .microphoneUnavailable:
            return "The selected microphone is not available."
        case .cameraPermissionDenied:
            return "Camera access is not allowed for MacPeripheralHub."
        case .cameraUnavailable:
            return "The selected camera is not available."
        }
    }
}

enum MediaPermissionKind: Equatable {
    case microphone
    case camera

    var title: String {
        switch self {
        case .microphone:
            return "Microphone Access"
        case .camera:
            return "Camera Access"
        }
    }

    fileprivate var mediaType: AVMediaType {
        switch self {
        case .microphone:
            return .audio
        case .camera:
            return .video
        }
    }

    fileprivate var settingsURL: URL? {
        switch self {
        case .microphone:
            return URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Microphone")
        case .camera:
            return URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Camera")
        }
    }
}

enum MediaPermissionStatus: Equatable {
    case unknown
    case notDetermined
    case allowed
    case denied
    case restricted

    var title: String {
        switch self {
        case .unknown:
            return "Unknown"
        case .notDetermined:
            return "Not Asked"
        case .allowed:
            return "Allowed"
        case .denied:
            return "Denied"
        case .restricted:
            return "Restricted"
        }
    }

    var detail: String {
        switch self {
        case .unknown:
            return "macOS did not return a permission status."
        case .notDetermined:
            return "Press Request to show the macOS permission prompt."
        case .allowed:
            return "Device checks can use this permission."
        case .denied:
            return "macOS will not show the prompt again. Open Privacy Settings and allow MacPeripheralHub."
        case .restricted:
            return "Access is restricted by macOS settings, Screen Time, MDM, or policy."
        }
    }

    fileprivate init(_ authorizationStatus: AVAuthorizationStatus) {
        switch authorizationStatus {
        case .notDetermined:
            self = .notDetermined
        case .authorized:
            self = .allowed
        case .denied:
            self = .denied
        case .restricted:
            self = .restricted
        @unknown default:
            self = .unknown
        }
    }
}

@MainActor
final class MediaTestController {
    private var systemSound: NSSound?
    private var soundEngine: AVAudioEngine?
    private var soundPlayer: AVAudioPlayerNode?
    private var soundPlaybackToken: UUID?
    private var microphoneWindows: [String: MicrophoneMonitorWindowController] = [:]
    private var cameraWindows: [String: CameraPreviewWindowController] = [:]

    func permissionStatus(for kind: MediaPermissionKind) -> MediaPermissionStatus {
        MediaPermissionStatus(AVCaptureDevice.authorizationStatus(for: kind.mediaType))
    }

    func requestPermission(for kind: MediaPermissionKind, completion: @escaping (MediaPermissionStatus) -> Void) {
        let status = permissionStatus(for: kind)
        switch status {
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: kind.mediaType) { granted in
                DispatchQueue.main.async {
                    completion(granted ? .allowed : .denied)
                }
            }
        case .denied, .restricted:
            openPrivacySettings(for: kind)
            completion(status)
        case .allowed, .unknown:
            completion(status)
        }
    }

    func openPrivacySettings(for kind: MediaPermissionKind) {
        guard let url = kind.settingsURL else {
            return
        }

        NSWorkspace.shared.open(url)
    }

    func playGlass(mono: Bool) throws {
        stopGlassPlayback()

        guard mono else {
            guard let sound = NSSound(named: "Glass") else {
                throw MediaTestError.glassSoundUnavailable
            }
            systemSound = sound
            sound.play()
            return
        }

        let url = URL(fileURLWithPath: "/System/Library/Sounds/Glass.aiff")
        let file = try AVAudioFile(forReading: url)
        let inputFormat = file.processingFormat
        guard let inputBuffer = AVAudioPCMBuffer(
            pcmFormat: inputFormat,
            frameCapacity: AVAudioFrameCount(file.length)
        ) else {
            throw MediaTestError.glassSoundUnavailable
        }
        try file.read(into: inputBuffer)

        guard let monoBuffer = makeMonoBuffer(from: inputBuffer) else {
            guard let sound = NSSound(named: "Glass") else {
                throw MediaTestError.glassSoundUnavailable
            }
            systemSound = sound
            sound.play()
            return
        }

        let engine = AVAudioEngine()
        let player = AVAudioPlayerNode()
        engine.attach(player)
        engine.connect(player, to: engine.mainMixerNode, format: monoBuffer.format)
        try engine.start()

        soundEngine = engine
        soundPlayer = player
        let playbackToken = UUID()
        soundPlaybackToken = playbackToken
        player.scheduleBuffer(monoBuffer, at: nil, options: []) { [weak self, playbackToken] in
            DispatchQueue.main.async {
                guard let self, self.soundPlaybackToken == playbackToken else {
                    return
                }
                self.stopGlassPlayback()
            }
        }
        player.play()
    }

    func showMicrophoneMonitor(device: DeviceViewModel, mono: Bool) {
        if let existing = microphoneWindows[device.id] {
            existing.showWindow(nil)
            existing.window?.makeKeyAndOrderFront(nil)
            existing.setMono(mono)
            return
        }

        let controller = MicrophoneMonitorWindowController(deviceName: device.displayName, mono: mono)
        controller.onClose = { [weak self] in
            Task { @MainActor in
                self?.microphoneWindows.removeValue(forKey: device.id)
            }
        }
        microphoneWindows[device.id] = controller
        controller.showWindow(nil)
        controller.window?.makeKeyAndOrderFront(nil)
        controller.start()
    }

    func showCameraPreview(device: DeviceViewModel) {
        if let existing = cameraWindows[device.id] {
            existing.showWindow(nil)
            existing.window?.makeKeyAndOrderFront(nil)
            return
        }

        let controller = CameraPreviewWindowController(
            deviceName: device.displayName,
            uniqueID: device.cameraUniqueID ?? cameraUniqueIDFallback(from: device.id)
        )
        controller.onClose = { [weak self] in
            Task { @MainActor in
                self?.cameraWindows.removeValue(forKey: device.id)
            }
        }
        cameraWindows[device.id] = controller
        controller.showWindow(nil)
        controller.window?.makeKeyAndOrderFront(nil)
        controller.start()
    }

    private func stopGlassPlayback() {
        systemSound?.stop()
        systemSound = nil
        soundPlayer?.stop()
        soundEngine?.stop()
        soundPlayer = nil
        soundEngine = nil
        soundPlaybackToken = nil
    }

    private func makeMonoBuffer(from inputBuffer: AVAudioPCMBuffer) -> AVAudioPCMBuffer? {
        guard let inputChannels = inputBuffer.floatChannelData,
              inputBuffer.frameLength > 0,
              inputBuffer.format.channelCount > 0,
              let outputFormat = AVAudioFormat(
                  commonFormat: .pcmFormatFloat32,
                  sampleRate: inputBuffer.format.sampleRate,
                  channels: 1,
                  interleaved: false
              ),
              let outputBuffer = AVAudioPCMBuffer(
                  pcmFormat: outputFormat,
                  frameCapacity: inputBuffer.frameLength
              ),
              let outputChannels = outputBuffer.floatChannelData else {
            return nil
        }

        outputBuffer.frameLength = inputBuffer.frameLength
        let frameCount = Int(inputBuffer.frameLength)
        let channelCount = Int(inputBuffer.format.channelCount)
        let output = outputChannels[0]

        for frameIndex in 0..<frameCount {
            var sum: Float = 0
            for channelIndex in 0..<channelCount {
                sum += inputChannels[channelIndex][frameIndex]
            }
            output[frameIndex] = sum / Float(channelCount)
        }

        return outputBuffer
    }

    private func cameraUniqueIDFallback(from deviceID: String) -> String {
        guard deviceID.hasPrefix("camera:") else {
            return deviceID
        }

        return String(deviceID.dropFirst("camera:".count))
    }
}

private final class LevelMeterView: NSView {
    var level: CGFloat = 0 {
        didSet {
            level = max(0, min(level, 1))
            needsDisplay = true
        }
    }

    override var intrinsicContentSize: NSSize {
        NSSize(width: 260, height: 14)
    }

    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        let bounds = self.bounds.insetBy(dx: 0, dy: 1)
        let backgroundPath = NSBezierPath(roundedRect: bounds, xRadius: 5, yRadius: 5)
        NSColor.separatorColor.withAlphaComponent(0.45).setFill()
        backgroundPath.fill()

        guard level > 0 else {
            return
        }

        let fillWidth = max(4, bounds.width * level)
        let fillRect = NSRect(x: bounds.minX, y: bounds.minY, width: fillWidth, height: bounds.height)
        let color: NSColor
        if level >= 0.84 {
            color = .systemRed
        } else if level >= 0.62 {
            color = .systemYellow
        } else {
            color = .systemGreen
        }
        color.setFill()
        NSBezierPath(roundedRect: fillRect, xRadius: 5, yRadius: 5).fill()
    }
}

@MainActor
private final class MicrophoneMonitorWindowController: NSWindowController, NSWindowDelegate {
    var onClose: (() -> Void)?

    private let deviceName: String
    private let statusLabel = NSTextField(labelWithString: "")
    private let levelStack = NSStackView()
    private let monoButton: NSButton
    private var mono: Bool
    private var engine: AVAudioEngine?
    private var meterViews: [LevelMeterView] = []
    private var meterValueLabels: [NSTextField] = []

    init(deviceName: String, mono: Bool) {
        self.deviceName = deviceName
        self.mono = mono
        self.monoButton = NSButton(checkboxWithTitle: "Mono", target: nil, action: nil)

        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 560, height: 330),
            styleMask: [.titled, .closable, .miniaturizable],
            backing: .buffered,
            defer: false
        )
        window.title = "Microphone Test - \(deviceName)"
        window.center()
        super.init(window: window)
        window.delegate = self
        configureContent()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("MicrophoneMonitorWindowController does not support storyboard initialization.")
    }

    func start() {
        ensurePermissionThenStart()
    }

    func setMono(_ mono: Bool) {
        guard self.mono != mono else {
            return
        }

        self.mono = mono
        monoButton.state = mono ? .on : .off
        restartEngine()
    }

    func windowWillClose(_ notification: Notification) {
        stopEngine()
        onClose?()
    }

    private func configureContent() {
        let title = NSTextField(labelWithString: deviceName)
        title.font = NSFont.systemFont(ofSize: 17, weight: .semibold)
        title.lineBreakMode = .byTruncatingMiddle

        statusLabel.font = NSFont.systemFont(ofSize: 12, weight: .regular)
        statusLabel.textColor = .secondaryLabelColor
        statusLabel.stringValue = "Preparing live monitoring..."

        monoButton.target = self
        monoButton.action = #selector(monoChanged(_:))
        monoButton.state = mono ? .on : .off
        monoButton.toolTip = "Monitor microphone as mono"

        let stopButton = NSButton(title: "Stop", target: self, action: #selector(stopFromButton))
        stopButton.bezelStyle = .rounded
        stopButton.image = NSImage(systemSymbolName: "stop.fill", accessibilityDescription: "Stop")
        stopButton.imagePosition = .imageLeading
        stopButton.toolTip = "Stop live monitoring"

        let header = NSStackView(views: [title, NSView(), monoButton, stopButton])
        header.orientation = .horizontal
        header.alignment = .centerY
        header.spacing = 10

        levelStack.orientation = .vertical
        levelStack.alignment = .width
        levelStack.spacing = 10
        configureMeters(channelCount: mono ? 1 : 2)

        let stack = NSStackView(views: [header, statusLabel, levelStack])
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 14
        stack.edgeInsets = NSEdgeInsets(top: 18, left: 18, bottom: 18, right: 18)
        window?.contentView = stack
    }

    private func ensurePermissionThenStart() {
        switch AVCaptureDevice.authorizationStatus(for: .audio) {
        case .authorized:
            restartEngine()
        case .notDetermined:
            statusLabel.stringValue = "Waiting for microphone permission..."
            AVCaptureDevice.requestAccess(for: .audio) { [weak self] granted in
                DispatchQueue.main.async {
                    guard let self else {
                        return
                    }
                    if granted {
                        self.restartEngine()
                    } else {
                        self.statusLabel.stringValue = MediaTestError.microphonePermissionDenied.localizedDescription
                    }
                }
            }
        case .denied, .restricted:
            statusLabel.stringValue = MediaTestError.microphonePermissionDenied.localizedDescription
        @unknown default:
            statusLabel.stringValue = MediaTestError.microphoneUnavailable.localizedDescription
        }
    }

    private func restartEngine() {
        stopEngine()

        do {
            try startEngine(useMono: mono)
        } catch {
            statusLabel.stringValue = error.localizedDescription
            configureMeters(channelCount: mono ? 1 : 2)
        }
    }

    private func startEngine(useMono: Bool) throws {
        let engine = AVAudioEngine()
        let input = engine.inputNode
        let inputFormat = input.outputFormat(forBus: 0)
        guard inputFormat.channelCount > 0 else {
            throw MediaTestError.microphoneUnavailable
        }

        let visibleChannelCount = useMono ? 1 : Int(inputFormat.channelCount)
        configureMeters(channelCount: visibleChannelCount)

        let mixer = AVAudioMixerNode()
        engine.attach(mixer)
        engine.connect(input, to: mixer, format: inputFormat)
        if useMono,
           let monoFormat = AVAudioFormat(
               commonFormat: .pcmFormatFloat32,
               sampleRate: inputFormat.sampleRate,
               channels: 1,
               interleaved: false
           ) {
            engine.connect(mixer, to: engine.mainMixerNode, format: monoFormat)
        } else {
            engine.connect(mixer, to: engine.mainMixerNode, format: inputFormat)
        }

        input.installTap(onBus: 0, bufferSize: 1024, format: inputFormat) { [weak self] buffer, _ in
            let levels = Self.levels(from: buffer, mono: useMono)
            DispatchQueue.main.async {
                self?.updateMeters(levels)
            }
        }

        try engine.start()
        self.engine = engine
        statusLabel.stringValue = useMono
            ? "Live monitoring active / mono / \(Int(inputFormat.sampleRate)) Hz"
            : "Live monitoring active / \(Int(inputFormat.channelCount)) channels / \(Int(inputFormat.sampleRate)) Hz"
    }

    private func stopEngine() {
        guard let engine else {
            return
        }

        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        self.engine = nil
        resetMeters()
    }

    private func configureMeters(channelCount: Int) {
        for view in levelStack.arrangedSubviews {
            levelStack.removeArrangedSubview(view)
            view.removeFromSuperview()
        }

        meterViews = []
        meterValueLabels = []
        let count = max(1, min(channelCount, 8))
        for index in 0..<count {
            let label = NSTextField(labelWithString: count == 1 ? "Mono" : "Channel \(index + 1)")
            label.font = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .medium)
            label.textColor = .secondaryLabelColor
            label.widthAnchor.constraint(equalToConstant: 74).isActive = true

            let meter = LevelMeterView()
            meter.translatesAutoresizingMaskIntoConstraints = false
            meter.heightAnchor.constraint(equalToConstant: 12).isActive = true
            meter.widthAnchor.constraint(greaterThanOrEqualToConstant: 260).isActive = true
            meterViews.append(meter)

            let valueLabel = NSTextField(labelWithString: "-inf dB")
            valueLabel.font = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .regular)
            valueLabel.textColor = .secondaryLabelColor
            valueLabel.alignment = .right
            valueLabel.widthAnchor.constraint(equalToConstant: 64).isActive = true
            meterValueLabels.append(valueLabel)

            let row = NSStackView(views: [label, meter, valueLabel])
            row.orientation = .horizontal
            row.alignment = .centerY
            row.spacing = 10
            levelStack.addArrangedSubview(row)
        }
        levelStack.needsLayout = true
        levelStack.layoutSubtreeIfNeeded()
        window?.contentView?.needsLayout = true
        window?.contentView?.layoutSubtreeIfNeeded()
    }

    private func updateMeters(_ levels: [Float]) {
        for (index, meter) in meterViews.enumerated() {
            let level = index < levels.count ? levels[index] : 0
            meter.level = CGFloat(level)
            if index < meterValueLabels.count {
                meterValueLabels[index].stringValue = Self.decibelText(for: level)
            }
        }
    }

    private func resetMeters() {
        for (index, meter) in meterViews.enumerated() {
            meter.level = 0
            if index < meterValueLabels.count {
                meterValueLabels[index].stringValue = "-inf dB"
            }
        }
    }

    @objc private func monoChanged(_ sender: NSButton) {
        mono = sender.state == .on
        restartEngine()
    }

    @objc private func stopFromButton() {
        window?.close()
    }

    private static func levels(from buffer: AVAudioPCMBuffer, mono: Bool) -> [Float] {
        guard let channels = buffer.floatChannelData,
              buffer.frameLength > 0,
              buffer.format.channelCount > 0 else {
            return [0]
        }

        let frameCount = Int(buffer.frameLength)
        let channelCount = Int(buffer.format.channelCount)
        var levels: [Float] = []
        levels.reserveCapacity(channelCount)

        for channelIndex in 0..<channelCount {
            let samples = channels[channelIndex]
            var sum: Float = 0
            for frameIndex in 0..<frameCount {
                let sample = samples[frameIndex]
                sum += sample * sample
            }
            levels.append(min(1, sqrt(sum / Float(frameCount)) * 4))
        }

        guard mono else {
            return levels
        }

        let average = levels.reduce(0, +) / Float(max(1, levels.count))
        return [average]
    }

    private static func decibelText(for level: Float) -> String {
        guard level > 0.0001 else {
            return "-inf dB"
        }

        let decibels = max(-60, min(0, 20 * log10(level)))
        return String(format: "%.0f dB", decibels)
    }
}

@MainActor
private final class CameraPreviewWindowController: NSWindowController, NSWindowDelegate {
    var onClose: (() -> Void)?

    private let deviceName: String
    private let uniqueID: String
    private let previewView = CameraPreviewView()
    private let statusLabel = NSTextField(labelWithString: "")
    private let sessionBox = CameraPreviewSessionBox()
    private let sessionQueue = DispatchQueue(label: "com.theandreyzakharov.MacPeripheralHub.cameraPreview")

    init(deviceName: String, uniqueID: String) {
        self.deviceName = deviceName
        self.uniqueID = uniqueID

        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 680, height: 440),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "Camera Preview - \(deviceName)"
        window.minSize = NSSize(width: 520, height: 340)
        window.center()
        super.init(window: window)
        window.delegate = self
        configureContent()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("CameraPreviewWindowController does not support storyboard initialization.")
    }

    func start() {
        ensurePermissionThenStart()
    }

    func windowWillClose(_ notification: Notification) {
        let sessionBox = self.sessionBox
        sessionQueue.async {
            sessionBox.stop()
        }
        onClose?()
    }

    private func configureContent() {
        let title = NSTextField(labelWithString: deviceName)
        title.font = NSFont.systemFont(ofSize: 15, weight: .semibold)
        title.lineBreakMode = .byTruncatingMiddle

        statusLabel.font = NSFont.systemFont(ofSize: 12, weight: .regular)
        statusLabel.textColor = .secondaryLabelColor
        statusLabel.stringValue = "Preparing camera preview..."

        previewView.translatesAutoresizingMaskIntoConstraints = false
        previewView.wantsLayer = true
        previewView.layer?.backgroundColor = NSColor.black.cgColor
        previewView.previewLayer.session = sessionBox.session

        let stack = NSStackView(views: [title, previewView, statusLabel])
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 10
        stack.edgeInsets = NSEdgeInsets(top: 14, left: 14, bottom: 14, right: 14)
        window?.contentView = stack

        previewView.heightAnchor.constraint(greaterThanOrEqualToConstant: 300).isActive = true
    }

    private func ensurePermissionThenStart() {
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            startSession()
        case .notDetermined:
            statusLabel.stringValue = "Waiting for camera permission..."
            AVCaptureDevice.requestAccess(for: .video) { [weak self] granted in
                DispatchQueue.main.async {
                    guard let self else {
                        return
                    }
                    if granted {
                        self.startSession()
                    } else {
                        self.statusLabel.stringValue = MediaTestError.cameraPermissionDenied.localizedDescription
                    }
                }
            }
        case .denied, .restricted:
            statusLabel.stringValue = MediaTestError.cameraPermissionDenied.localizedDescription
        @unknown default:
            statusLabel.stringValue = MediaTestError.cameraUnavailable.localizedDescription
        }
    }

    private func startSession() {
        statusLabel.stringValue = "Starting preview..."
        let sessionBox = self.sessionBox
        let uniqueID = self.uniqueID
        sessionQueue.async { [weak self, sessionBox, uniqueID] in

            do {
                let device = AVCaptureDevice(uniqueID: uniqueID) ??
                    AVCaptureDevice.default(for: .video)
                guard let device else {
                    throw MediaTestError.cameraUnavailable
                }

                try sessionBox.start(device: device)

                DispatchQueue.main.async {
                    guard let self else {
                        return
                    }
                    self.statusLabel.stringValue = "Preview active"
                }
            } catch {
                DispatchQueue.main.async {
                    guard let self else {
                        return
                    }
                    self.statusLabel.stringValue = error.localizedDescription
                }
            }
        }
    }
}

private final class CameraPreviewSessionBox: @unchecked Sendable {
    let session = AVCaptureSession()

    func start(device: AVCaptureDevice) throws {
        let input = try AVCaptureDeviceInput(device: device)
        session.beginConfiguration()
        session.sessionPreset = .high
        for existingInput in session.inputs {
            session.removeInput(existingInput)
        }
        if session.canAddInput(input) {
            session.addInput(input)
        } else {
            session.commitConfiguration()
            throw MediaTestError.cameraUnavailable
        }
        session.commitConfiguration()
        session.startRunning()
    }

    func stop() {
        if session.isRunning {
            session.stopRunning()
        }
    }
}

private final class CameraPreviewView: NSView {
    let previewLayer = AVCaptureVideoPreviewLayer()

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        wantsLayer = true
        previewLayer.videoGravity = .resizeAspect
        layer?.addSublayer(previewLayer)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("CameraPreviewView does not support storyboard initialization.")
    }

    override func layout() {
        super.layout()
        previewLayer.frame = bounds
    }
}
