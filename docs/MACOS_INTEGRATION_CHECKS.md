# macOS Integration Checks

Date: 2026-07-19
App: `MacPeripheralHub`
Version: `1.0.0`

This file records the macOS integration checks for the 1.0.0 development pass. Hardware-dependent scenarios must be repeated on a real desktop setup with the target USB microphone and Bluetooth headphones connected.

## Confirmed In This Workspace

| Check | Status | Evidence |
| --- | --- | --- |
| Debug app build | Confirmed | `scripts/build_app.sh` completed with `** BUILD SUCCEEDED **`. |
| App launch from built Xcode product | Confirmed | `scripts/run_app.sh` built `Debug/MacPeripheralHub.app` and launched it with `open`; `pgrep -x MacPeripheralHub` returned PID `71160` during the run. |
| Standalone release build | Confirmed | `scripts/build_release.sh` completed with `** BUILD SUCCEEDED **` and runs `scripts/test_core.sh` first. |
| Bundle metadata | Confirmed | Built app has `CFBundleName = MacPeripheralHub`, `CFBundleShortVersionString = 1.0.0`, `CFBundleVersion = 1`, and `CFBundleIconName = AppIcon`. |
| Bundle resources | Confirmed | Built app contains `Contents/Resources/AppIcon.icns` and `Contents/Resources/Assets.car`. |
| CoreAudio live adapter smoke | Confirmed | `make test-core` completed and exercised CoreAudio enumeration/default role reads plus missing-device handling. |
| Display/HID/USB/Bluetooth/camera live adapter smoke | Confirmed | `make test-core` completed live smoke paths. Camera permission was denied on this Mac and enumeration correctly skipped cameras without failing the full test. |
| Quit through app termination path | Confirmed | `scripts/stop_app.sh` sent the app quit command; a following `pgrep -x MacPeripheralHub` returned no process. |

`xcodebuild` emitted CoreSimulator and local sandbox warnings in this environment, but the macOS app build itself succeeded.

## Covered By Core Simulation

| Scenario | Status | Coverage |
| --- | --- | --- |
| Bluetooth headphones reset default input | Covered by C-core test | `test_reconcile_airpods_reset` verifies that a desired USB microphone is restored after current input changes to an AirPods-like device. |
| Missing selected device | Covered by C-core test | `test_reconcile_missing_device_retry` verifies missing-device marking, retry debounce, retry cap, and later recovery when the device reappears. |
| Manual switch does not mutate saved profile | Covered by C-core test | `test_reconcile_manual_override` verifies manual output selection while the saved profile remains unchanged. |
| USB/HID duplicate physical device | Covered by C-core test | Inventory snapshot test verifies a USB shell device merging into a HID keyboard entry. |
| Profile and active selection persistence | Covered by C-core test | SQLite tests verify profile CRUD, active profile/manual state roundtrip, known devices, and migrations. |

## Requires Manual Hardware Verification

Run these on the target Mac setup before the final release pass:

- Connect a USB microphone and confirm it appears under `Devices` and `Manual Control`.
- Connect Bluetooth headphones with a microphone and confirm input/output devices appear.
- Select the USB microphone as default input, then connect Bluetooth headphones and confirm MacPeripheralHub restores the USB microphone.
- Disconnect the selected USB microphone and confirm the UI marks it missing/offline without crashing.
- Reconnect the selected USB microphone and confirm reconciliation can select it again.
- Put the Mac to sleep, wake it, and confirm inventory refresh plus reconciliation still run.
- Close the main window using the red close button and confirm the app remains in the menu bar.
- Open the menu bar menu and use `Quit`; confirm the process exits.

## Source-Level Checks For Manual Items

- `MainWindowController.windowShouldClose` orders the window out, keeps the app running, and calls the app-level Dock hide callback.
- `AppDelegate.hideApplicationFromDock` switches activation policy to `.accessory`.
- `StatusMenuController` owns the `NSStatusItem`, exposes quick device/profile actions, `Open MacPeripheralHub`, and `Quit`.
- `BackgroundServiceController` listens for wake/session/display/app events and keeps the audio watcher/reconciliation loop alive while the app is running.
