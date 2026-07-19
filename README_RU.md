<div align="center">

<img src="assets/forreadme/logo.png" alt="Логотип MacPeripheralHub" width="300"/>

# MacPeripheralHub

[![Русский](https://img.shields.io/badge/README_Language-Русский-brightgreen)](https://github.com/TheAndreyZakharov/MacPeripheralHub/blob/main/README_RU.md)
[![English](https://img.shields.io/badge/README_Language-English-blue)](https://github.com/TheAndreyZakharov/MacPeripheralHub/blob/main/README.md)

</div>

MacPeripheralHub — это macOS-приложение для просмотра подключенной периферии, быстрого переключения активных аудиоустройств и удержания выбранных системных audio defaults.

Приложение сделано как обычное desktop-приложение macOS с настоящим окном, пунктом в menu bar, фоновыми watchers, хранением профилей и прямой интеграцией с системными API macOS.

Когда главное окно закрывается красной кнопкой, MacPeripheralHub пропадает из Dock и продолжает работать из menu bar рядом с часами macOS.

Планируемая публичная версия приложения — `1.0.0`.

## Назначение

MacPeripheralHub сделан для Mac-сетапов, где в течение дня подключается, отключается, соединяется или просыпается много устройств.

macOS и отдельные приложения могут менять default-микрофон или выход после появления гарнитуры, док-станции, монитора, USB-хаба или Bluetooth-устройства.

Например, пользователь может выбрать USB-микрофон как default input, позже подключить AirPods, и macOS может переключить default input на микрофон AirPods.

MacPeripheralHub удерживает выбранный audio input, audio output и system output, отслеживая системные изменения и возвращая нужные устройства.

## Поддерживаемые устройства

Inventory view рассчитан на показ максимального количества подключенной периферии, которую macOS отдает приложениям.

Категории устройств:

- мониторы и встроенные экраны;
- микрофоны;
- наушники, колонки и другие аудиовыходы;
- аудиоинтерфейсы и звуковые карты;
- камеры и веб-камеры;
- клавиатуры;
- мыши;
- трекпады;
- Bluetooth-устройства;
- USB-устройства;
- USB-хабы и док-станции;
- неизвестные или частично определенные устройства.

Для каждого устройства приложение показывает лучшие доступные данные: имя, категорию, производителя, модель, серийный номер, transport, статус подключения и характеристики роли.

Для мониторов приложение может показывать разрешение, частоту обновления, статус main display и доступную информацию о подключении.

Для аудиоустройств приложение может показывать каналы, sample rate и является ли устройство текущим default input, output или system output.

Для камер, HID-устройств, USB-устройств и Bluetooth-устройств приложение показывает идентификаторы и metadata, когда macOS их предоставляет.

## Главная функция

Главный защищаемый сценарий в версии `1.0.0` — стабильное управление audio defaults.

MacPeripheralHub хранит либо активный профиль, либо текущий ручной выбор.

Когда macOS меняет default input, output или system output с выбранного устройства на другое, reconciliation engine решает, нужно ли приложению вернуть выбранное устройство.

Приложение использует debounce и retry-поведение, чтобы не дергать macOS слишком резко, пока устройства еще появляются после подключения, wake или Bluetooth pairing.

Ручное переключение не перезаписывает сохраненные профили. Оно переводит active state в `Manual Control`, чтобы пользователь мог временно сменить устройства без разрушения сохраненного сетапа.

## Проверка устройств

В `Devices` встроены проверки для media-периферии.

Для наушников, колонок и аудиовыходов нажмите `Glass`, чтобы проиграть системный звук macOS Glass через выбранный output.

У проверки audio output есть опция `Mono` для центрированного mono playback.

Для микрофонов нажмите `Listen`, чтобы открыть live monitoring с meters уровней по каналам.

В окне микрофона тоже есть опция `Mono` для mono monitoring и mono level display.

Для камер нажмите `Preview`, чтобы открыть среднее окно preview для проверки кадра, фокуса и экспозиции.

Проверки микрофона и камеры запрашивают macOS permissions только когда пользователь запускает проверку.

`Settings` показывает статус permissions микрофона и камеры с действиями для повторного запроса доступа или открытия нужной страницы macOS Privacy Settings.

## Установка из GitHub Release

Готовое macOS-приложение планируется публиковать в разделе GitHub Releases этого репозитория.

Скачайте assets последнего релиза из GitHub Releases.

В релизе должны быть и application bundle, и файл контрольных сумм.

Ожидаемые release assets:

    MacPeripheralHub.app.zip
    MacPeripheralHub.app.zip.sha256

Положите `MacPeripheralHub.app.zip` и `MacPeripheralHub.app.zip.sha256` в одну папку.

Проверьте архив:

    shasum -a 256 -c MacPeripheralHub.app.zip.sha256

Распакуйте архив:

    unzip MacPeripheralHub.app.zip

При необходимости переместите приложение в `Applications`:

    mv "MacPeripheralHub.app" /Applications/

При первом запуске macOS может предупредить, что приложение не подписано и не notarized.

В этом случае нажмите по приложению правой кнопкой мыши и выберите `Открыть`.

Если quarantine мешает запуску, удалите quarantine-атрибут:

    xattr -cr "MacPeripheralHub.app"

или, если приложение уже перемещено в Applications:

    xattr -cr "/Applications/MacPeripheralHub.app"

## Скриншоты приложения

## Главные экраны

`Manual Control` — первый рабочий экран для мгновенного переключения активных устройств.

Он показывает текущий default-микрофон, аудиовыход, system output, active mode и preferred camera selection.

`Devices` показывает полный inventory, сгруппированный по категориям устройств.

Он нужен, чтобы быстро понять, что подключено к Mac и какие metadata удалось определить.

`Profiles` хранит переиспользуемые комбинации устройств.

Каждый профиль может хранить выбранные audio defaults, preferred camera, ожидаемую периферию и настройку, которая говорит MacPeripheralHub удерживать выбранные аудиоустройства активными.

Пункт в menu bar показывает компактный current state, быстрое переключение audio, активацию профилей, действие открытия окна и `Quit`.

## Ограничения macOS

macOS дает приложениям надежный системный API для смены default audio input, default audio output и default system output.

macOS не дает такого же универсального принудительного default camera для всех приложений.

MacPeripheralHub хранит preferred camera в профилях и показывает ее в UI, но финальный выбор камеры все равно может зависеть от приложения, которое использует камеру.

Клавиатуры, мыши, трекпады, USB-хабы, док-станции и многие обычные USB или HID-устройства можно обнаруживать и описывать, но обычно нельзя сделать глобально активными так же, как аудиоустройства.

Приложение документирует permissions и privacy behavior в `docs/PRIVACY.md`.

Заметки по hardware verification записаны в `docs/MACOS_INTEGRATION_CHECKS.md`.

## Сборка из исходников

Требования:

- macOS 13 или новее;
- Xcode с command line tools;
- системная SQLite library, которая входит в macOS.

Собрать, проверить и создать локальный release app bundle:

    scripts/package_app.sh

Скрипт запускает все локальные проверки и затем записывает:

    dist/MacPeripheralHub.app
    dist/MacPeripheralHub.app.zip
    dist/MacPeripheralHub.app.zip.sha256

Checksum-файл проверяет release zip archive.

Та же упаковка доступна через Make:

    make package-app

## Локальный запуск

Запустить debug build из репозитория:

    scripts/run_app.sh

Остановить запущенное приложение:

    scripts/stop_app.sh

Собрать только debug-приложение:

    scripts/build_app.sh

Собрать только release-приложение:

    scripts/build_release.sh

## Тесты

Запустить C-core tests:

    make test-core

Запустить полный локальный build and test flow:

    scripts/test_all.sh

`scripts/package_app.sh` тоже запускает `scripts/test_all.sh` перед копированием release-приложения в `dist`.

## Структура проекта

    MacPeripheralHub/        AppKit-приложение, окно, menu bar, Dock behavior и Swift system bridge
    Core/include/            Публичные C headers для core library
    Core/src/                C core, Objective-C system adapters, SQLite storage, inventory и reconciliation
    Core/tests/              C smoke, unit, mapper, storage и reconciliation tests
    Core/fixtures/           Искусственные device fixtures для core tests
    Storage/migrations/      SQLite schema migrations
    scripts/                 Build, run, stop, test и package commands
    docs/                    Product notes, roadmap, privacy notes и integration checks
    assets/forreadme/        README assets

## Стек технологий

- Swift и AppKit для macOS application shell и UI.
- C для core models, profile logic, matching, reconciliation и tests.
- SQLite для persistent profiles, known devices, aliases и active state.
- CoreAudio для audio enumeration и default audio switching.
- AVFoundation для camera enumeration.
- CoreGraphics и IOKit для displays и system hardware metadata.
- IOHIDManager для keyboards, mice, trackpads и другой HID-периферии.
- IOBluetooth для Bluetooth device metadata, где это доступно.

## Архитектура

Swift отвечает за пользовательский интерфейс, menu bar integration, app lifecycle, Dock behavior и high-level application state.

C core отвечает за устойчивую продуктовую логику: device models, stable matching, profile data, active selections, SQLite persistence и reconciliation decisions.

System adapters собирают live macOS state и конвертируют его в core device records.

Reconciliation loop сравнивает desired state с current state и применяет только поддерживаемые audio changes.

Такое разделение оставляет macOS shell нативным, а самую большую часть репозитория — в тестируемом C-коде.

## Данные и приватность

MacPeripheralHub хранит профили и known-device metadata локально в пользовательской директории Application Support.

Для основного workflow приложению не нужен network access.

Приложение может запрашивать у macOS permissions, связанные с камерой или микрофоном, только там, где этого требуют системные API.

Если permission отклонен, приложение должно продолжать работу и показывать ту информацию об устройствах, которую macOS все еще разрешает.

App bundle подписывается с camera и audio-input entitlements, чтобы macOS могла показывать MacPeripheralHub в Privacy Settings.

## Статус версии

Текущая целевая версия: `1.0.0`.

Приложение должно собираться в полноценный macOS `.app` bundle с названием `MacPeripheralHub.app`.

Некоторые hardware scenarios все еще требуют финальной ручной проверки на реальном сетапе с целевым USB-микрофоном, Bluetooth-наушниками, мониторами, хабами и sleep/wake behavior.
