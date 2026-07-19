# Privacy And Permissions

MacPeripheralHub is a local macOS utility. Version `1.0.0` does not send device inventory, profile data, audio, video, serial numbers, Bluetooth addresses, or USB/HID identifiers to any network service.

## Permission Matrix

| Area | API | Permission behavior |
| --- | --- | --- |
| Audio input/output inventory | CoreAudio hardware properties | No microphone capture is started. The app reads device metadata and current default audio roles. |
| Audio default switching | CoreAudio default device properties | No microphone permission prompt is required because the app sets system default device identifiers, not audio streams. |
| Camera inventory | AVFoundation discovery session | `NSCameraUsageDescription` is present. If camera access is denied or restricted, camera enumeration is skipped and the rest of inventory continues. |
| Camera default selection | macOS limitation | macOS does not expose a reliable global default camera setter. Profiles store a preferred camera only. |
| Displays | CoreGraphics and IOKit display registry | No usage description is required. |
| Keyboard, mouse, trackpad and HID inventory | IOHIDManager | No Input Monitoring permission is requested because the app reads device metadata and does not observe keystrokes or pointer input. |
| USB devices, hubs and docks | IOKit USB registry | No usage description is required for metadata enumeration. |
| Bluetooth devices | IOBluetooth paired/recent device APIs | No Bluetooth permission is requested by the app in `1.0.0`; unavailable metadata is treated as missing/unknown. |
| Launch at login | ServiceManagement `SMAppService.mainApp` | macOS may require user approval in System Settings. The app stores that the prompt was shown and does not repeatedly nag. |

## Current App Privacy Strings

- `NSCameraUsageDescription`: used if macOS requires camera permission while discovering connected cameras.
- `NSMicrophoneUsageDescription`: included defensively for macOS privacy transparency, but `1.0.0` does not request microphone capture permission or record audio.

## Entitlements

- The project does not define a custom entitlements file in `1.0.0`.
- Debug builds signed by Xcode include `com.apple.security.get-task-allow` for local debugging.
- App Sandbox is not enabled because the app needs broad local hardware inventory through CoreAudio, IOKit, AVFoundation and IOBluetooth.
- No network, automation, input monitoring, Bluetooth, USB, camera, or microphone entitlements are added by the project.

## Failure Policy

- Permission denial must not crash the app.
- Denied/restricted camera access produces an empty camera section instead of failing the whole inventory refresh.
- Missing USB/HID/Bluetooth metadata is shown as `unknown` or omitted from details.
- Audio reconciliation remains focused on CoreAudio default input, default output and default system output.
- Unsupported control surfaces, such as a global default camera or forcing a specific keyboard/mouse as active, are represented as preferences or inventory only.
