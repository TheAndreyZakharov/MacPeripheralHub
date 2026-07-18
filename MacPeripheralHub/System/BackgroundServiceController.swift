import AppKit
import Combine

@MainActor
final class BackgroundServiceController {
    private let core: PeripheralCoreBridge
    private weak var appState: AppState?
    private var audioWatcher: PeripheralAudioWatcher?
    private var cancellables = Set<AnyCancellable>()
    private var observers: [NSObjectProtocol] = []
    private var pendingRefresh: DispatchWorkItem?
    private var isStarted = false
    private var lastErrorSignature: String?

    init(core: PeripheralCoreBridge, appState: AppState) {
        self.core = core
        self.appState = appState
    }

    func start() {
        guard !isStarted else {
            return
        }

        isStarted = true
        observeActiveSelection()
        observeSystemNotifications()
        startAudioWatcher()
        log("background services started")
    }

    func stop() {
        pendingRefresh?.cancel()
        pendingRefresh = nil
        cancellables.removeAll()

        for observer in observers {
            NotificationCenter.default.removeObserver(observer)
            NSWorkspace.shared.notificationCenter.removeObserver(observer)
        }
        observers.removeAll()

        audioWatcher?.stop()
        audioWatcher = nil
        isStarted = false
        log("background services stopped")
    }

    func requestImmediateReconciliation(reason: String) {
        log("requesting reconciliation: \(reason)")
        do {
            try audioWatcher?.requestManualScan()
        } catch {
            appState?.reportBackgroundServiceError(error, operation: "Request background reconciliation")
        }
    }

    private func startAudioWatcher() {
        guard let appState else {
            return
        }

        do {
            let watcher = try core.makeAudioWatcher(desiredSelection: appState.activeSelection) { [weak self] event in
                DispatchQueue.main.async {
                    self?.handleAudioWatcherEvent(event)
                }
            }
            try watcher.start()
            audioWatcher = watcher
        } catch {
            appState.reportBackgroundServiceError(error, operation: "Start background services")
        }
    }

    private func observeActiveSelection() {
        appState?.$activeSelection
            .removeDuplicates()
            .receive(on: RunLoop.main)
            .sink { [weak self] selection in
                self?.syncDesiredSelection(selection)
            }
            .store(in: &cancellables)
    }

    private func observeSystemNotifications() {
        let workspaceCenter = NSWorkspace.shared.notificationCenter
        observers.append(
            workspaceCenter.addObserver(
                forName: NSWorkspace.didWakeNotification,
                object: nil,
                queue: .main
            ) { [weak self] _ in
                self?.reloadStateAndReconcile(reason: "wake from sleep")
            }
        )
        observers.append(
            workspaceCenter.addObserver(
                forName: NSWorkspace.sessionDidBecomeActiveNotification,
                object: nil,
                queue: .main
            ) { [weak self] _ in
                self?.reloadStateAndReconcile(reason: "user session became active")
            }
        )
        observers.append(
            NotificationCenter.default.addObserver(
                forName: NSApplication.didBecomeActiveNotification,
                object: nil,
                queue: .main
            ) { [weak self] _ in
                self?.reloadStateAndReconcile(reason: "application became active")
            }
        )
        observers.append(
            NotificationCenter.default.addObserver(
                forName: NSApplication.didChangeScreenParametersNotification,
                object: nil,
                queue: .main
            ) { [weak self] _ in
                self?.scheduleRefresh(reason: "display configuration changed")
            }
        )
    }

    private func syncDesiredSelection(_ selection: SelectionViewModel) {
        do {
            if let audioWatcher {
                try core.updateAudioWatcher(audioWatcher, desiredSelection: selection)
            }
            log("synced desired selection: mode=\(selection.mode.rawValue)")
        } catch {
            appState?.reportBackgroundServiceError(error, operation: "Sync background desired state")
        }
    }

    private func handleAudioWatcherEvent(_ event: AudioWatcherEventViewModel) {
        log("audio watcher event: \(event.summary)")

        if event.shouldRefreshInventory {
            scheduleRefresh(reason: "audio watcher event")
        }

        if event.didFailReconcile {
            reportWatcherFailure(event)
        } else if event.didApplyReconcile {
            lastErrorSignature = nil
        }
    }

    private func reloadStateAndReconcile(reason: String) {
        log("reloading state: \(reason)")
        appState?.refreshAll()
        syncDesiredSelection(appState?.activeSelection ?? .empty)
        requestImmediateReconciliation(reason: reason)
    }

    private func scheduleRefresh(reason: String) {
        pendingRefresh?.cancel()
        let workItem = DispatchWorkItem { [weak self] in
            guard let self else {
                return
            }
            self.log("refreshing state: \(reason)")
            self.appState?.refreshAll()
        }
        pendingRefresh = workItem
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.25, execute: workItem)
    }

    private func reportWatcherFailure(_ event: AudioWatcherEventViewModel) {
        let signature = "\(event.statusName):\(event.flags):\(event.actionCount)"
        guard signature != lastErrorSignature else {
            return
        }
        lastErrorSignature = signature
        appState?.reportBackgroundServiceError(
            PeripheralCoreBridgeError(
                operation: "Background audio reconciliation",
                statusName: event.statusName
            ),
            operation: "Background audio reconciliation"
        )
    }

    private func log(_ message: String) {
        NSLog("[MacPeripheralHub] %@", message)
    }
}
