<div align="center">

<img src="assets/forreadme/logo.png" alt="MacPeripheralHub logo" width="300"/>

# MacPeripheralHub

[![Русский](https://img.shields.io/badge/README_Language-Русский-blue)](https://github.com/TheAndreyZakharov/MacPeripheralHub/blob/main/README_RU.md)
[![English](https://img.shields.io/badge/README_Language-English-brightgreen)](https://github.com/TheAndreyZakharov/MacPeripheralHub/blob/main/README.md)

</div>

MacPeripheralHub is a macOS application for seeing connected peripheral devices, switching active audio devices quickly, and keeping selected system audio defaults stable.

The application is built as a normal macOS desktop app with a real window, a menu bar item, background watchers, profile storage, and direct integration with macOS system APIs.

When the main window is closed with the red close button, MacPeripheralHub disappears from the Dock and continues working from the menu bar near the macOS clock.

The planned public application version is `1.0.0`.

## Purpose

MacPeripheralHub is made for Mac setups where many devices are connected, disconnected, paired, or awakened during the day.

macOS and individual applications can change the default microphone or output after a headset, dock, monitor, USB hub, or Bluetooth device appears.

For example, a user can choose a USB microphone as the default input, connect AirPods later, and macOS may move the default input to the AirPods microphone.

MacPeripheralHub keeps the chosen audio input, audio output, and system output selected by watching system changes and restoring the desired devices.

## Supported devices

The inventory view is designed to show as much connected peripheral hardware as macOS exposes.

Device categories:

- displays and built-in screens;
- microphones;
- headphones, speakers, and other audio outputs;
- audio interfaces and sound cards;
- cameras and webcams;
- keyboards;
- mice;
- trackpads;
- Bluetooth devices;
- USB devices;
- USB hubs and docking stations;
- unknown or partially identified devices.

For every device, the app shows the best available details: name, category, manufacturer, model, serial number, transport, connection status, and role-specific characteristics.

For displays, the app can show resolution, refresh rate, main display status, and available connection metadata.

For audio devices, the app can show channels, sample rate, and whether the device is the current default input, output, or system output.

For cameras, HID devices, USB devices, and Bluetooth devices, the app shows identifiers and metadata when macOS provides them.

## Core feature

The main protected workflow in version `1.0.0` is stable audio default control.

MacPeripheralHub stores either an active profile or the current manual selection.

When macOS changes the default input, output, or system output away from the selected device, the reconciliation engine decides whether the app should restore it.

The app uses debounce and retry behavior so it does not fight macOS too aggressively while devices are still appearing after connection, wake, or Bluetooth pairing.

Manual switching does not overwrite saved profiles. It moves the active state into `Manual Control`, so the user can change devices for the moment without destroying a saved setup.

## Device checks

The `Devices` view includes built-in checks for media peripherals.

For headphones, speakers, and audio outputs, press `Glass` to play the macOS Glass system sound through the selected output.

The audio output check has a `Mono` option for centered mono playback.

For microphones, press `Listen` to open live monitoring with per-channel level meters.

The microphone monitor also has a `Mono` option for mono monitoring and mono level display.

For cameras, press `Preview` to open a medium preview window for checking framing, focus, and exposure.

Microphone and camera checks request macOS permissions only when the user starts the check.

`Settings` shows microphone and camera permission status with actions to request access again or open the exact macOS Privacy Settings page.

## Installation from GitHub Release

The ready-to-use macOS application is intended to be available in the repository's GitHub Releases section.

Download the latest release assets from GitHub Releases.

The release should include both the application bundle and its checksum file.

Expected release assets:

    MacPeripheralHub.app.zip
    MacPeripheralHub.app.zip.sha256

Place `MacPeripheralHub.app.zip` and `MacPeripheralHub.app.zip.sha256` in the same folder.

Verify the archive:

    shasum -a 256 -c MacPeripheralHub.app.zip.sha256

Extract the archive:

    unzip MacPeripheralHub.app.zip

Move the application to `Applications` if desired:

    mv "MacPeripheralHub.app" /Applications/

On first launch, macOS may warn that the application is not signed or notarized.

In that case, right-click the application and choose `Open`.

If quarantine blocks launch, remove the quarantine attribute:

    xattr -cr "MacPeripheralHub.app"

or, after moving it to Applications:

    xattr -cr "/Applications/MacPeripheralHub.app"

## Application walkthrough

This section shows the main workflows and the places where MacPeripheralHub controls or explains connected devices.

### Manual Control

<div align="center">
<img src="assets/forreadme/1.png" alt="Manual Control tab" width="600"/>
</div>

`Manual Control` is the first tab and the fastest place to set current defaults manually.

It lets the user choose the active input, output, system output, and preferred camera without changing saved profiles.

After a manual switch, the app keeps the current state as manual control until another profile is activated.

### Devices

<div align="center">
<img src="assets/forreadme/2.png" alt="Devices tab" width="600"/>
</div>

`Devices` shows all connected hardware that macOS exposes to the app, including wired, wireless, audio, video, USB, Bluetooth, display, HID, hub, and unknown devices.

Devices are grouped by category, with expandable sections and detected metadata for understanding what each device is.

Media devices can be checked directly: microphones can be monitored live, outputs can play the Glass sound, and cameras can be previewed.

### Profiles

<div align="center">
<img src="assets/forreadme/3.png" alt="Profiles tab" width="600"/>
</div>

`Profiles` is for reusable device combinations for different tasks, rooms, calls, streams, or work scenarios.

A profile can store selected input, output, system output, preferred camera, and expected peripherals.

Profiles can be created, edited, deleted, and activated quickly when the user needs to switch the whole setup.

### Settings

<div align="center">
<img src="assets/forreadme/4.png" alt="Settings tab" width="600"/>
</div>

`Settings` contains application-level options and system permission controls.

The appearance selector can follow macOS automatically or force light or dark mode for the app.

The same view can request microphone and camera access again, open Privacy Settings, and enable or disable launch at login.

### Dark Appearance

<div align="center">
<img src="assets/forreadme/5.png" alt="Dark appearance" width="600"/>
</div>

The dark appearance keeps the same workflow and layout while using macOS dark colors.

It can be selected manually in `Settings` or inherited from the system when appearance is set to `System`.

This makes the app comfortable to leave running while it watches audio defaults in the background.

### Menu Bar

<div align="center">
<img src="assets/forreadme/6.jpeg" alt="Menu bar item" width="600"/>
</div>

The menu bar item keeps MacPeripheralHub available after the main window is closed.

It shows compact status, active defaults, quick switching, profile activation, a command to reopen the window, and `Quit`.

This is the main background control surface when the app is running without a Dock window.

## macOS limitations

macOS gives applications a reliable system API for changing default audio input, default audio output, and default system output.

macOS does not provide the same universal forced default camera setting for all applications.

MacPeripheralHub stores a preferred camera in profiles and displays it in the UI, but the final camera selection can still depend on the application that uses the camera.

Keyboard, mouse, trackpad, USB hub, dock, and many generic USB or HID devices can be detected and described, but usually cannot be made globally active in the same way as audio devices.

The app documents permission and privacy behavior in `docs/PRIVACY.md`.

Hardware verification notes are recorded in `docs/MACOS_INTEGRATION_CHECKS.md`.

## Build from source

Requirements:

- macOS 13 or newer;
- Xcode with command line tools;
- the system SQLite library included with macOS.

Build, test, and create a local release app bundle:

    scripts/package_app.sh

The script runs the full local checks and then writes:

    dist/MacPeripheralHub.app
    dist/MacPeripheralHub.app.zip
    dist/MacPeripheralHub.app.zip.sha256

The checksum file verifies the release zip archive.

The same packaging step is available through Make:

    make package-app

## Running locally

Run the debug build from the repository:

    scripts/run_app.sh

Stop the running application:

    scripts/stop_app.sh

Build the debug app only:

    scripts/build_app.sh

Build the release app only:

    scripts/build_release.sh

## Tests

Run C-core tests:

    make test-core

Run the full local build and test flow:

    scripts/test_all.sh

`scripts/package_app.sh` also runs `scripts/test_all.sh` before copying the release app into `dist`.

## Project structure

    MacPeripheralHub/        AppKit application, window, menu bar, Dock behavior, and Swift system bridge
    Core/include/            Public C headers for the core library
    Core/src/                C core, Objective-C system adapters, SQLite storage, inventory, and reconciliation
    Core/tests/              C smoke, unit, mapper, storage, and reconciliation tests
    Core/fixtures/           Synthetic device fixtures for core tests
    Storage/migrations/      SQLite schema migrations
    scripts/                 Build, run, stop, test, and package commands
    docs/                    Product notes, roadmap, privacy notes, and integration checks
    assets/forreadme/        README assets

## Technology stack

- Swift and AppKit for the macOS application shell and UI.
- C for the core models, profile logic, matching, reconciliation, and tests.
- SQLite for persistent profiles, known devices, aliases, and active state.
- CoreAudio for audio enumeration and default audio switching.
- AVFoundation for camera enumeration.
- CoreGraphics and IOKit for displays and system hardware metadata.
- IOHIDManager for keyboards, mice, trackpads, and other HID peripherals.
- IOBluetooth for Bluetooth device metadata where available.

## Architecture

Swift owns the user interface, menu bar integration, app lifecycle, Dock behavior, and high-level application state.

The C core owns the durable product logic: device models, stable matching, profile data, active selections, SQLite persistence, and reconciliation decisions.

System adapters collect live macOS state and convert it into core device records.

The reconciliation loop compares desired state with current state and applies only the supported audio changes.

This split keeps the macOS shell native while leaving the largest part of the repository in testable C code.

## Data and privacy

MacPeripheralHub stores profiles and known-device metadata locally in the user's Application Support directory.

The app does not need network access for its core workflow.

The app may ask macOS for camera or microphone-related permissions only where system APIs require them.

If permission is denied, the app should keep working and show whatever device information macOS still allows.

The app bundle is signed with camera and audio-input entitlements so macOS can show MacPeripheralHub in Privacy Settings.

## Version status

Current target version: `1.0.0`.

The app is intended to build into a full macOS `.app` bundle named `MacPeripheralHub.app`.

Some hardware scenarios still require final manual verification on a real setup with the target USB microphone, Bluetooth headphones, displays, hubs, and sleep/wake behavior.
