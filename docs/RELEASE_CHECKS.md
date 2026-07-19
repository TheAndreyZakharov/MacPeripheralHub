# Release Checks 1.0.0

Date: 2026-07-19
App: `MacPeripheralHub`
Version: `1.0.0`

This document records the final release-readiness pass for the local `1.0.0` build.

## Build And Package

| Check | Status | Evidence |
| --- | --- | --- |
| Full package command | Passed | `scripts/package_app.sh` completed successfully. |
| Debug build | Passed | The package command ran the Debug app build first. |
| C-core tests | Passed | `build/core/test_core_smoke` printed `PeripheralCore smoke tests passed`. |
| Release build | Passed | The package command completed the Release app build with `** BUILD SUCCEEDED **`. |
| Release app artifact | Passed | `dist/MacPeripheralHub.app` was created. |
| Release checksum file | Passed | `dist/MacPeripheralHub.app.zip.sha256` was created. |
| Checksum verification | Passed | `shasum -a 256 -c MacPeripheralHub.app.zip.sha256` returned `OK` for the release archive. |
| Local code signature | Passed | `codesign --verify --deep --strict --verbose=2 dist/MacPeripheralHub.app` reported the app as valid on disk. |

`xcodebuild` prints CoreSimulator-related warnings in this sandboxed environment, but the macOS application target builds successfully.

## Bundle Metadata

| Field | Expected | Confirmed |
| --- | --- | --- |
| `CFBundleDisplayName` | `MacPeripheralHub` | `MacPeripheralHub` |
| `CFBundleExecutable` | `MacPeripheralHub` | `MacPeripheralHub` |
| `CFBundleName` | `MacPeripheralHub` | `MacPeripheralHub` |
| `CFBundleShortVersionString` | `1.0.0` | `1.0.0` |
| `CFBundleVersion` | `1` | `1` |
| `CFBundleIdentifier` | `com.theandreyzakharov.MacPeripheralHub` | `com.theandreyzakharov.MacPeripheralHub` |
| `LSMinimumSystemVersion` | `13.0` | `13.0` |
| `CFBundleIconName` | `AppIcon` | `AppIcon` |

The built app bundle contains:

- `Contents/Resources/AppIcon.icns`
- `Contents/Resources/Assets.car`

## Runtime Checks

| Check | Status | Evidence |
| --- | --- | --- |
| App launch | Passed | `scripts/run_app.sh` built and launched the Debug app. |
| Process is running after launch | Passed | `pgrep -x MacPeripheralHub` returned PID `91612`. |
| Stop script quits app | Passed | `scripts/stop_app.sh` completed and a following `pgrep -x MacPeripheralHub` returned no process. |
| Menu bar controller exists | Passed by source check | `StatusMenuController` creates an `NSStatusItem`, quick audio actions, profile actions, open-window action, and `Quit`. |
| Window close hides window and Dock icon | Passed by source check | `MainWindowController.windowShouldClose` orders the window out and calls `AppDelegate.hideApplicationFromDock`, which sets `.accessory`. |
| App remains running after last window closes | Passed by source check | `applicationShouldTerminateAfterLastWindowClosed` returns `false`. |
| Login item prompt is not repeated endlessly | Passed by source check | `LoginItemController` stores `MacPeripheralHub.launchAtLoginPromptShown`; `AppDelegate` marks it before acting on the prompt. |

The current AppKit app is not scriptable through `tell application "MacPeripheralHub" to close every window`, so the red-button behavior is verified through source-level checks plus the app lifecycle implementation.

## Repository Language Balance

Tracked C, Objective-C, and header lines:

```text
8543
```

Tracked Swift lines:

```text
3562
```

The repository contains substantially more C-family core code than Swift UI code.

## Git State

Before the release-check documentation edits, `git status --short --ignored` showed only ignored generated directories:

```text
!! build/
!! dist/
!! venv/
```

After committing this release-readiness pass, the same generated directories are expected to remain ignored and the tracked tree should be clean.
