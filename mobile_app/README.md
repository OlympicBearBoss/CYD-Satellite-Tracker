# Flutter Mobile App

Цель: единый Flutter-клиент для iPhone, iPad и Android.

## План модулей
```
lib/
  app/
  core/network/       # HTTP + WebSocket
  core/models/
  features/home/
  features/monitoring/
  features/rotator/
  features/satellites/
  features/radio/
  features/diagnostics/
  features/settings/
```

## Подключение
1. Домашняя Wi-Fi сеть: CYD в режиме station.
2. Прямое подключение: CYD создаёт точку доступа.

На iOS и Android нужно отдельно обработать системные разрешения и особенности переключения Wi-Fi.

## Статус
Исходный Flutter-код пока не добавлен. Здесь зафиксированы структура и требования.
