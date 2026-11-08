# Авторизация CYD Satellite Tracker

## Целевая схема

- Cloudflare Pages Functions — HTTP API авторизации.
- Cloudflare D1 — пользователи, внешние идентификаторы, сессии и привязанные приложения.
- Email + пароль, Google OpenID Connect и Sign in with Apple.
- Мобильное приложение получает отдельную пару токенов после одноразовой привязки устройства; пароль пользователя в приложение не передаётся.

## Подготовка Cloudflare

1. Создать D1 database с именем `cyd-satellite-tracker-auth`.
2. Вставить полученный database ID вместо `REPLACE_WITH_D1_DATABASE_ID` в `wrangler.toml`.
3. Применить миграцию: `wrangler d1 migrations apply cyd-satellite-tracker-auth --remote`.
4. Убедиться, что Pages Functions проекта используют D1 binding с именем `DB`.

## Google

1. В Google Cloud Console создать OAuth Client ID типа Web application.
2. Добавить production origin сайта в Authorized JavaScript origins.
3. Добавить callback URL, который будет указан в настройке OAuth API проекта, в Authorized redirect URIs.
4. Сохранить Client ID и Client Secret как Cloudflare Pages secrets, не в репозитории.

## Apple

1. Нужна активная учётная запись Apple Developer Program.
2. Создать App ID и включить Sign in with Apple.
3. Создать Services ID для веб-входа и связать его с primary App ID.
4. Настроить домен сайта и return URL в Apple Developer.
5. Создать Sign in with Apple key (.p8); сохранить Key ID, Team ID, Services ID и приватный ключ только в Cloudflare secrets.

## Обязательные секреты

Имена переменных окружения должны быть согласованы с обработчиком OAuth перед включением кнопок:
- GOOGLE_CLIENT_ID
- GOOGLE_CLIENT_SECRET
- APPLE_CLIENT_ID (Services ID)
- APPLE_TEAM_ID
- APPLE_KEY_ID
- APPLE_PRIVATE_KEY

Не публикуйте секреты, приватный ключ Apple, пароли или реальные токены в GitHub, переписке или клиентском JavaScript.

## Состояние

Эта папка задаёт схему хранения и конфигурацию D1. Для включения входа необходимы OAuth-реквизиты и реализация Pages Functions API; одна только миграция не включает регистрацию или вход.
