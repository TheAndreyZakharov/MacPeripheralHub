# Roadmap MacPeripheralHub 1.0.0

Цель: собрать полноценное macOS-приложение `MacPeripheralHub` версии `1.0.0`, которое показывает всю подключенную периферию, дает быстро переключать активные устройства, хранит профили и постоянно удерживает выбранные системные audio default-устройства.

Главный технический принцип: Swift/AppKit делает shell приложения и UI, а большая часть основной логики, моделей, хранения, проверок и тестов живет в C, чтобы репозиторий уверенно определялся как C-проект.

## 0. Базовые правила разработки

- [ ] Не коммитить автоматически из Codex.
- [ ] Каждый пункт с `Commit:` коммитит пользователь вручную после проверки.
- [ ] Держать версию приложения `1.0.0` с первого рабочего build.
- [ ] Не делать MVP-режим: каждая стадия должна оставлять проект в аккуратном, собираемом состоянии.
- [ ] Не смешивать большие независимые изменения в один коммит.
- [ ] Любой системный API оборачивать в C-слой или тонкий adapter, если это возможно без грязной архитектуры.
- [ ] Все persistent-данные хранить через SQLite.
- [ ] Все небезопасные macOS-ограничения явно отражать в UI и логике, а не притворяться, что можно переключать то, что macOS системно не дает переключать.

## 1. Создание структуры проекта

- [x] Создать Xcode/macOS проект `MacPeripheralHub`.
- [x] Тип приложения: macOS App.
- [x] UI stack: AppKit + Swift.
- [x] Minimum macOS выбрать осознанно, желательно macOS 13+, чтобы использовать `SMAppService` для Login Items.
- [x] Добавить bundle id, например `com.theandreyzakharov.MacPeripheralHub`.
- [x] Настроить app version `1.0.0` и build number `1`.
- [x] Создать структуру директорий:
  - `MacPeripheralHub/App`
  - `MacPeripheralHub/UI`
  - `MacPeripheralHub/MenuBar`
  - `MacPeripheralHub/Dock`
  - `MacPeripheralHub/System`
  - `MacPeripheralHub/Resources`
  - `Core/include`
  - `Core/src`
  - `Core/tests`
  - `Core/fixtures`
  - `Storage/migrations`
  - `scripts`
  - `docs`
- [x] Добавить `.clang-format` для C-кода.
- [x] Добавить базовый `Makefile` для сборки и тестов C-core отдельно от Xcode.
- [x] Добавить стартовые scripts для build, release build, run и stop приложения.
- [x] Проверить, что пустой app запускается и показывает окно.

Commit: `chore: scaffold macOS app and C core layout`

## 2. Настройка C-core как главной части проекта

- [x] Создать статическую библиотеку `PeripheralCore`.
- [x] Подключить C-library к Swift target через bridging/module map.
- [x] Добавить публичные headers в `Core/include`.
- [x] Добавить базовые C-модули:
  - `mph_device`
  - `mph_device_id`
  - `mph_device_list`
  - `mph_profile`
  - `mph_profile_store`
  - `mph_selection`
  - `mph_reconcile`
  - `mph_result`
  - `mph_log`
  - `mph_time`
- [x] Добавить единый style ошибок: `mph_status_t`, error code, optional message buffer.
- [x] Добавить memory ownership rules в headers.
- [x] Добавить smoke-тесты C-core.
- [x] Подключить `make test-core`.

Commit: `feat(core): add C foundation library`

## 3. SQLite-хранилище

- [x] Подключить системную `libsqlite3`.
- [x] Создать слой `mph_db`.
- [x] Добавить миграции в `Storage/migrations`.
- [x] Создать таблицу `profiles`.
- [x] Создать таблицу `profile_device_roles`.
- [x] Создать таблицу `known_devices`.
- [x] Создать таблицу `active_state`.
- [x] Создать таблицу `schema_migrations`.
- [x] Реализовать открытие базы в Application Support.
- [x] Реализовать migration runner.
- [x] Реализовать CRUD профилей.
- [x] Реализовать сохранение последнего active profile или manual state.
- [x] Покрыть миграции и CRUD C-тестами.

Commit: `feat(storage): add SQLite profile database`

## 4. Модель устройств и категорий

- [x] Описать enum категорий:
  - display
  - audio_input
  - audio_output
  - audio_system_output
  - camera
  - keyboard
  - mouse
  - trackpad
  - bluetooth
  - usb
  - hub
  - dock
  - audio_interface
  - unknown
- [x] Описать enum transport:
  - built_in
  - usb
  - bluetooth
  - thunderbolt
  - hdmi
  - display_port
  - virtual
  - aggregate
  - unknown
- [x] Описать `mph_device_t` с stable id, display name, vendor, model, serial, category, transport, connection state.
- [x] Описать typed characteristics для audio/display/camera/HID/Bluetooth/USB.
- [x] Реализовать нормализацию имен устройств.
- [x] Реализовать matching устройств между запусками.
- [x] Добавить fixtures с искусственными устройствами.
- [x] Покрыть matching и categorization C-тестами.

Commit: `feat(core): model peripheral devices and matching`

## 5. CoreAudio: аудиоустройства

- [x] Создать C/Objective-C adapter для CoreAudio.
- [x] Получать список audio devices.
- [x] Разделять input, output и system output capable devices.
- [x] Читать device UID.
- [x] Читать имя устройства.
- [x] Читать manufacturer, если доступен.
- [x] Читать sample rate.
- [x] Читать channel count.
- [x] Читать current default input.
- [x] Читать current default output.
- [x] Читать current default system output.
- [x] Реализовать установку default input.
- [x] Реализовать установку default output.
- [x] Реализовать установку default system output.
- [x] Обработать исчезновение выбранного устройства без падения.
- [x] Добавить unit tests на mapper и integration smoke на реальной машине, где возможно.

Commit: `feat(system): enumerate and switch CoreAudio devices`

## 6. Reconciliation engine

- [x] Реализовать C-модуль `mph_reconcile`.
- [x] На вход принимать desired state из active profile/manual control.
- [x] На вход принимать current state из системных adapters.
- [x] Вычислять действия: no-op, set default input, set default output, set system output, mark missing.
- [x] Не трогать сохраненный профиль при ручном переключении.
- [x] Добавить manual state как отдельный active mode.
- [x] Добавить debounce, чтобы не дергать macOS слишком часто.
- [x] Добавить retry policy для устройств, которые появились с задержкой.
- [x] Добавить tests для сценария: подключили AirPods, macOS сбросила input, приложение вернуло USB mic.
- [x] Добавить tests для missing device.
- [x] Добавить tests для manual override.

Commit: `feat(core): enforce selected audio defaults`

## 7. Watchers системных изменений

- [x] Подписаться на CoreAudio property listeners.
- [x] Обновлять state при изменении default input/output/system output.
- [x] Обновлять state при появлении/исчезновении audio devices.
- [x] Добавить fallback periodic scan.
- [x] Синхронизировать watchers с reconciliation engine.
- [x] Сделать thread-safe dispatch из system callbacks в app state.
- [x] Добавить логирование системных событий.

Commit: `feat(system): watch audio device changes`

## 8. Displays

- [x] Создать adapter для CoreGraphics/IOKit display APIs.
- [x] Получать список подключенных дисплеев.
- [x] Определять built-in/external.
- [x] Читать разрешение.
- [x] Читать refresh rate, если доступен.
- [x] Читать main display flag.
- [x] Читать connection/vendor/model metadata через IOKit, если доступно.
- [x] Категоризировать HDMI/DisplayPort/Thunderbolt по доступным данным.
- [x] Показывать неизвестный connection type как `unknown`, если macOS не отдала точную информацию.
- [x] Добавить mapper tests на fixtures.

Commit: `feat(system): enumerate connected displays`

## 9. Cameras

- [x] Создать adapter для AVFoundation capture devices.
- [x] Получать список video devices.
- [x] Читать localized name.
- [x] Читать unique id.
- [x] Читать manufacturer/model metadata, если доступно.
- [x] Читать transport type, если доступно.
- [x] Отмечать, что глобальный forced default camera в macOS недоступен.
- [x] Сохранять preferred camera в профиле как preference.
- [x] Покрыть mapper tests.

Commit: `feat(system): enumerate cameras`

## 10. HID: клавиатуры, мыши, трекпады

- [x] Создать adapter для IOHIDManager.
- [x] Получать HID devices.
- [x] Определять keyboard/mouse/trackpad по usage page и usage.
- [x] Читать vendor id.
- [x] Читать product id.
- [x] Читать product name.
- [x] Читать manufacturer.
- [x] Читать transport.
- [x] Читать serial, если доступен.
- [x] Убирать дубли, если одно устройство видно через несколько API.
- [x] Добавить categorization tests.

Commit: `feat(system): enumerate HID peripherals`

## 11. USB, hubs, docks и неизвестные устройства

- [x] Создать adapter для IOKit USB registry.
- [x] Получать USB devices tree.
- [x] Определять hubs.
- [x] Определять docks, если metadata позволяет.
- [x] Читать vendor id, product id, vendor name, product name, serial.
- [x] Читать speed/power metadata, если доступно.
- [x] Связывать USB audio/camera/HID devices с уже найденными устройствами, где возможно.
- [x] Все неполно определенные устройства класть в `unknown` или `usb`.
- [x] Добавить deduplication tests.

Commit: `feat(system): enumerate USB devices and hubs`

## 12. Bluetooth

- [x] Создать adapter для IOBluetooth.
- [x] Получать paired devices.
- [x] Получать connected devices, если доступно.
- [x] Читать name.
- [x] Читать address.
- [x] Читать class of device, если доступно.
- [x] Связывать Bluetooth audio/HID devices с CoreAudio/HID entries, где возможно.
- [x] Не показывать один и тот же девайс дважды без причины.
- [x] Добавить tests для merge логики.

Commit: `feat(system): enumerate Bluetooth peripherals`

## 13. Aggregated device inventory

- [x] Создать `mph_inventory`.
- [x] Объединять данные из CoreAudio, Displays, Cameras, HID, USB, Bluetooth.
- [x] Убирать дубли по stable id, serial, uid, transport metadata.
- [x] Сохранять known devices в SQLite.
- [x] Помечать currently connected/disconnected.
- [x] Сортировать устройства по категории и имени.
- [x] Добавить snapshot tests на искусственный набор устройств.

Commit: `feat(core): aggregate peripheral inventory`

## 14. App state bridge Swift <-> C

- [x] Создать Swift wrapper над C-core.
- [x] Конвертировать C structs в Swift view models.
- [x] Изолировать unsafe pointers.
- [x] Добавить единый `AppState`.
- [x] Добавить async refresh inventory.
- [x] Добавить activation profile flow.
- [x] Добавить manual selection flow.
- [x] Добавить error presentation model.

Commit: `feat(app): bridge C core into Swift app state`

## 15. Окно приложения

- [x] Реализовать основное окно AppKit.
- [x] Сделать нормальное macOS-приложение, а не только menu bar utility.
- [x] Добавить sidebar/tabs:
  - Manual Control
  - Devices
  - Profiles
- [x] Настроить минимальный размер окна.
- [x] Сохранить размер и позицию окна между запусками.
- [x] Добавить пустые состояния для случаев, когда устройства не найдены.
- [x] Добавить аккуратные loading/error состояния.

Commit: `feat(ui): add main application window`

## 16. Manual Control UI

- [x] Показать текущий active mode: profile или manual.
- [x] Показать current default input.
- [x] Показать current default output.
- [x] Показать current default system output.
- [x] Добавить picker микрофона.
- [x] Добавить picker аудиовыхода.
- [x] Добавить picker system output.
- [x] Добавить preferred camera picker как preference.
- [x] При выборе устройства переводить состояние в `Manual Control`.
- [x] После выбора сразу применять системный default, если это аудиороль.
- [x] Показывать missing/offline devices в понятном виде.

Commit: `feat(ui): add manual device controls`

## 17. Devices UI

- [x] Показать категории устройств секциями.
- [x] Для каждого устройства показать имя, transport, manufacturer/model.
- [x] Для дисплеев показать resolution/refresh rate/main flag.
- [x] Для аудиоустройств показать channels/sample rate/default role flags.
- [x] Для камер показать unique id/transport.
- [x] Для HID/USB/Bluetooth показать ids и connection metadata.
- [x] Добавить поиск или фильтр по устройствам.
- [x] Добавить refresh button.
- [x] Сделать текст компактным, но читаемым.

Commit: `feat(ui): add categorized device inventory`

## 18. Profiles UI

- [ ] Показать список профилей.
- [ ] Добавить создание профиля.
- [ ] Добавить переименование профиля.
- [ ] Добавить редактирование профиля.
- [ ] Добавить удаление профиля.
- [ ] Добавить выбор устройств по ролям.
- [ ] Для каждой роли показывать только подходящие устройства.
- [ ] Позволить сохранить missing device, если профиль был создан раньше, а устройство сейчас отключено.
- [ ] Добавить кнопку `Activate`.
- [ ] Показывать активный профиль.
- [ ] Не перезаписывать профиль при manual switch.

Commit: `feat(ui): add profile management`

## 19. Menu bar

- [ ] Добавить `NSStatusItem`.
- [ ] Добавить иконку приложения в menu bar.
- [ ] В menu bar показать компактный current state.
- [ ] Добавить quick switch input device.
- [ ] Добавить quick switch output device.
- [ ] Добавить quick switch system output.
- [ ] Добавить список профилей для быстрой активации.
- [ ] Добавить `Open MacPeripheralHub`.
- [ ] Добавить `Quit`.
- [ ] Обновлять menu bar при подключении/отключении устройств.
- [ ] Убедиться, что menu bar остается после закрытия окна.

Commit: `feat(menubar): add status item controls`

## 20. Закрытие окна и Dock behavior

- [ ] При нажатии красной кнопки закрывать/прятать окно.
- [ ] После закрытия окна переводить app activation policy в accessory, чтобы приложение не мешало в Dock.
- [ ] При открытии окна из menu bar возвращать normal activation policy.
- [ ] Убедиться, что приложение продолжает watchers/reconciliation в фоне.
- [ ] Реализовать Dock menu с быстрыми профилями, когда приложение видно в Dock.
- [ ] Не обещать Dock menu после скрытия из Dock: в этом состоянии быстрый доступ идет через menu bar.

Commit: `feat(app): hide Dock icon when window closes`

## 21. Login item prompt

- [ ] На первом запуске показать предложение включить запуск при входе.
- [ ] Использовать `SMAppService` на macOS 13+.
- [ ] Сохранить, что prompt уже показывался.
- [ ] Добавить настройку включения/выключения login item в приложении.
- [ ] Обработать отказ пользователя без повторного навязчивого prompt.
- [ ] Добавить понятный error state, если macOS не разрешила включить login item.

Commit: `feat(app): add launch at login support`

## 22. Background service loop

- [ ] Запускать inventory watchers при старте приложения.
- [ ] Запускать reconciliation loop при старте приложения.
- [ ] Поддерживать active profile/manual state даже без открытого окна.
- [ ] При wake from sleep делать refresh и reconciliation.
- [ ] При смене пользователя/аудиосессии корректно перечитывать state.
- [ ] Добавить logs для диагностики.
- [ ] Не делать busy polling.

Commit: `feat(app): keep selected devices active in background`

## 23. Иконка, ресурсы и polish

- [ ] Добавить app icon.
- [ ] Добавить menu bar template icon.
- [ ] Добавить аккуратные empty/loading/error states.
- [ ] Добавить About window с версией `1.0.0`.
- [ ] Проверить название приложения во всех системных местах.
- [ ] Проверить Info.plist metadata.
- [ ] Добавить privacy usage descriptions, если требуются для камер/микрофонов.

Commit: `chore(app): add app resources and metadata`

## 24. Permissions и privacy

- [ ] Проверить, какие API требуют entitlements или usage descriptions.
- [x] Добавить camera usage description, если AVFoundation enumeration требует доступ.
- [ ] Проверить microphone permission behavior.
- [ ] Не запрашивать лишние permissions без необходимости.
- [ ] Документировать ограничения по камерам и HID/USB управлению.
- [ ] Убедиться, что приложение не падает при отказанных permissions.

Commit: `chore(app): configure permissions and privacy strings`

## 25. Тесты C-core

- [ ] Покрыть SQLite migrations.
- [ ] Покрыть profile CRUD.
- [ ] Покрыть device matching.
- [ ] Покрыть deduplication.
- [ ] Покрыть reconciliation decisions.
- [ ] Покрыть manual control behavior.
- [ ] Покрыть missing device behavior.
- [ ] Покрыть serialization/deserialization.
- [ ] Сделать `make test-core` обязательной проверкой перед релизом.

Commit: `test(core): cover profiles inventory and reconciliation`

## 26. Интеграционные проверки macOS

- [ ] Проверить запуск app из Xcode.
- [ ] Проверить standalone build.
- [ ] Проверить обнаружение встроенного микрофона и динамиков.
- [ ] Проверить подключение USB-микрофона.
- [ ] Проверить подключение Bluetooth-наушников.
- [ ] Проверить сценарий сброса default input после подключения Bluetooth-наушников.
- [ ] Проверить, что приложение возвращает выбранный USB-микрофон.
- [ ] Проверить отключение выбранного устройства.
- [ ] Проверить повторное подключение выбранного устройства.
- [ ] Проверить работу после sleep/wake.
- [ ] Проверить работу после закрытия окна.
- [ ] Проверить Quit из menu bar.

Commit: `test(app): verify macOS device switching flows`

## 27. Build scripts

- [x] Добавить `scripts/build_debug.sh`.
- [x] Добавить `scripts/build_app.sh`.
- [x] Добавить `scripts/build_release.sh`.
- [x] Добавить `scripts/run_app.sh`.
- [x] Добавить `scripts/stop_app.sh`.
- [x] Добавить `scripts/test_core.sh`.
- [ ] Добавить `scripts/test_all.sh`.
- [ ] Добавить `scripts/package_app.sh`, если нужен локальный `.app` artifact.
- [ ] Сделать scripts понятными и без скрытых глобальных зависимостей.

Commit: `chore(scripts): add build and test commands`

## 28. README

- [ ] Описать назначение приложения.
- [ ] Описать поддерживаемые устройства.
- [ ] Описать главную функцию удержания audio defaults.
- [ ] Описать ограничения macOS по камерам/HID/USB.
- [ ] Описать сборку проекта.
- [ ] Описать запуск тестов.
- [ ] Описать архитектуру Swift + C + SQLite.
- [ ] Добавить статус версии `1.0.0`.

Commit: `docs: document MacPeripheralHub usage and architecture`

## 29. Release readiness 1.0.0

- [ ] Проверить, что app version `1.0.0`.
- [ ] Проверить, что build number корректный.
- [ ] Проверить, что приложение называется `MacPeripheralHub`.
- [ ] Проверить, что bundle содержит app icon.
- [ ] Проверить, что закрытие окна убирает Dock icon.
- [ ] Проверить, что menu bar остается живым.
- [ ] Проверить, что Quit полностью завершает процесс.
- [ ] Проверить, что login item prompt не повторяется бесконечно.
- [ ] Проверить, что C-кода в репозитории больше, чем Swift-кода.
- [ ] Проверить, что `make test-core` проходит.
- [ ] Проверить, что release build собирается.
- [ ] Проверить чистоту `git status`.

Commit: `release: prepare MacPeripheralHub 1.0.0`

## 30. Рекомендуемый порядок первых рабочих коммитов

- [x] `chore: scaffold macOS app and C core layout`
- [x] `feat(core): add C foundation library`
- [x] `feat(storage): add SQLite profile database`
- [x] `feat(core): model peripheral devices and matching`
- [x] `feat(system): enumerate and switch CoreAudio devices`
- [x] `feat(core): enforce selected audio defaults`
- [x] `feat(system): watch audio device changes`
- [x] `feat(system): enumerate connected displays`
- [x] `feat(system): enumerate cameras`
- [x] `feat(system): enumerate HID peripherals`
- [x] `feat(system): enumerate USB devices and hubs`
- [x] `feat(system): enumerate Bluetooth peripherals`
- [x] `feat(core): aggregate peripheral inventory`
- [ ] `feat(app): bridge C core into Swift app state`
- [ ] `feat(ui): add main application window`
- [ ] `feat(ui): add manual device controls`
- [ ] `feat(ui): add categorized device inventory`
- [ ] `feat(ui): add profile management`
- [ ] `feat(menubar): add status item controls`
- [ ] `feat(app): hide Dock icon when window closes`
- [ ] `feat(app): add launch at login support`
- [ ] `feat(app): keep selected devices active in background`
- [ ] `chore(app): add app resources and metadata`
- [ ] `chore(app): configure permissions and privacy strings`
- [ ] `test(core): cover profiles inventory and reconciliation`
- [ ] `test(app): verify macOS device switching flows`
- [ ] `chore(scripts): add build and test commands`
- [ ] `docs: document MacPeripheralHub usage and architecture`
- [ ] `release: prepare MacPeripheralHub 1.0.0`
