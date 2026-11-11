Локальное Wi-Fi подключение CYD

Архитектура: контроллер ESP32 и клиент (браузер или Flutter-приложение) находятся в одной локальной Wi-Fi сети. ESP32 поднимает HTTP API на порту 80 и страницу управления на корневом адресе.

Браузер: открыть http://cyd-tracker.local/ или http://IP-АДРЕС-CYD/.
Статус: GET /api/v1/status.
Команда: POST /api/v1/command с JSON {"command":"stop"}.
mDNS: cyd-tracker.local, сервис _http._tcp.

HTTPS-сайт Cloudflare не должен незаметно выполнять HTTP-запросы к локальному адресу: браузеры ограничивают такие запросы (mixed content / private network access). Для гарантированного браузерного локального управления открывайте страницу, размещённую непосредственно на ESP32. Сайт аккаунта может показывать адрес и инструкцию подключения.

Flutter: каталог app_flutter содержит начальный клиент для Android, iOS и iPadOS. Введите IP контроллера либо http://cyd-tracker.local. Клиент читает тот же /api/v1/status и отправляет команду остановки.

Для подготовки платформенных проектов выполните flutter create . из app_flutter, затем flutter pub get. Для Android разрешить локальный HTTP-трафик (usesCleartextTraffic). Для iOS добавить NSLocalNetworkUsageDescription и Bonjour service _http._tcp в Info.plist.

Ограничения текущей версии: API отдаёт телеметрию Wi-Fi/GPS и статус облачной привязки. Команда stop пока только подтверждается прошивкой; шаговые двигатели остаются заблокированными. Движение нельзя включать до реализации и проверки концевиков, аварийной остановки и защиты механики. API в этой версии открыт внутри локальной сети без пароля. Облачная привязка и heartbeat остаются независимыми и не заменяются локальным API.


## Автопоиск в мобильном приложении

Приложение ищет DNS-SD объявления службы `_http._tcp.local`, получает IPv4 адрес и проверяет
`GET /api/v1/status`. В список попадают только устройства, ответившие с именем
`CYD Satellite Tracker`. Поиск выполняется в той же Wi-Fi сети; при необходимости
адрес можно ввести вручную.

### Разрешения платформ

Проект в репозитории пока содержит исходники Flutter, но не сгенерированные каталоги
платформ. Перед первой сборкой выполните из каталога `app_flutter`:

```bash
flutter create --platforms=android,ios .
```

В `android/app/src/main/AndroidManifest.xml` внутри `<manifest>` добавьте:

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

В `<application>` добавьте атрибут `android:usesCleartextTraffic="true"`, поскольку
локальный API контроллера использует HTTP в домашней сети.

В `ios/Runner/Info.plist` добавьте:

```xml
<key>NSLocalNetworkUsageDescription</key>
<string>Приложение ищет CYD Satellite Tracker в вашей локальной Wi-Fi сети.</string>
<key>NSBonjourServices</key>
<array><string>_http._tcp</string></array>
```

iOS покажет системный запрос доступа к локальной сети при первом поиске.
