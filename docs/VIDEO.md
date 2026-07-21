# 00:00–00:05 - Заставка
    MacPeripheralHub
    CS50x 2026 Final Project
    Andrey Zakharov
    GitHub: TheAndreyZakharov
    edX: TheAndreyZakharov
    Moscow, Russia
    Recorded: [MONTH DAY, 2026]



# 00:05–00:20 - Представление проблемы
Hi, my name is Andrey Zakharov, and this is MacPeripheralHub — my final project for Harvard’s CS50x 2026.

My Mac is connected to multiple monitors, cameras, microphones, speakers, Bluetooth devices, and USB hubs. Throughout the day, these devices may be connected, disconnected, or wake up from sleep, and macOS can unexpectedly change the default microphone or audio output.



# 00:20–01:00 - Представление решения
I created MacPeripheralHub to bring the management of this setup into one place. However, the application is not limited to my own workspace. It is designed for anyone with a large peripheral setup or anyone who regularly experiences the same device-switching problems.

It allows users to quickly switch between device profiles and keep selected audio devices active in the background. This means that the microphone and audio outputs chosen by the user remain the system defaults, even when the hardware configuration changes.

Now, let me show you how it works.



# 01:00–01:20 - Manual Control
The Manual Control tab displays the current input, output, system output, and preferred camera. From here, I can immediately switch an individual device without changing any of my saved profiles. I have also opened the macOS System Settings window so you can see that the selected devices are actually changing at the system level.



# 01:20–01:40 - Devices
The Devices tab creates a unified view of connected hardware using macOS system APIs for audio devices, displays, cameras, USB, HID, and Bluetooth. It shows the available metadata and provides tools for testing audio outputs, microphones, and cameras, as well as reviewing all currently connected devices.



# 01:40–01:55 - Profiles
Profiles store predefined combinations of devices for different situations, such as work, video calls, or streaming. When a profile is activated, the supported audio devices are switched together, while the application also remembers the preferred camera and the expected set of peripherals.



# 01:55–02:20 - Audio defoults and menu bar
MacPeripheralHub also monitors system changes. If macOS replaces a selected audio device after a device is reconnected or the computer wakes from sleep, the reconciliation engine can automatically restore the required default device.

After the main window is closed, the application continues running through the menu bar. From there, users can access the essential controls, view the current device state, and quickly switch between profiles without reopening the full application.



# 02:20–02:35 - Architecture
Architecturally, the application uses Swift and AppKit for the native macOS interface and system integration. A narrow bridge connects this layer to a C core containing the device models, profile logic, matching algorithms, reconciliation decisions, and tests. SQLite provides reliable local storage without requiring a server or network connection.



# 02:35–03:00 - Final
The goal of this project is simple: keep your preferred devices exactly the way you configured them.
MacPeripheralHub is available on GitHub with its complete source code and a ready-to-use build. I hope you enjoyed the project, and perhaps someone facing similar peripheral-management problems will find it useful.

CS50x was a challenging and genuinely rewarding experience.

Thank you for watching. This was CS50.