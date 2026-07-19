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

## Application screenshots

## Main views

`Manual Control` is the first working view for switching active devices immediately.

It shows the current default microphone, audio output, system output, active mode, and preferred camera selection.

`Devices` shows the full inventory grouped by device category.

It is meant for quickly understanding what is connected to the Mac and which metadata was detected.

`Profiles` stores reusable combinations of devices.

Each profile can keep selected audio defaults, preferred camera, expected peripherals, and the setting that tells MacPeripheralHub to keep the selected audio devices active.

The menu bar item exposes compact current state, quick audio switching, profile activation, an action to open the window, and `Quit`.

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

## Version status

Current target version: `1.0.0`.

The app is intended to build into a full macOS `.app` bundle named `MacPeripheralHub.app`.

Some hardware scenarios still require final manual verification on a real setup with the target USB microphone, Bluetooth headphones, displays, hubs, and sleep/wake behavior.
