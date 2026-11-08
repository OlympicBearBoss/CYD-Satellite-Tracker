# Авторизация CYD Satellite Tracker

## Текущий этап

Реализованы серверные Pages Functions для регистрации по email/паролю, входа, выхода и проверки текущей сессии. Пароли хешируются PBKDF2-SHA-256 (310 000 итераций, индивидуальная соль); в D1 хранится хеш случайного идентификатора сессии, а браузер получает только Secure, HttpOnly, SameSite=Lax cookie.

**Это пока серверная основа, не включённая в production.** До запуска необходимо создать D1, заменить placeholder database ID в `wrangler.toml`, применить миграцию и проверить API на staging. Также до публичной регистрации следует добавить подтверждение email, восстановление пароля, защиту от массовых попыток (Cloudflare rate limiting/Turnstile) и журналирование безопасности.

## Google

Google-вход оставлен следующим этапом настройки: нужны OAuth Client ID и Client Secret, зарегистрированный callback URL и реализация полного OIDC flow с проверкой подписи ID token, issuer, audience, expiry и nonce. Кнопку Google нельзя включать до настройки этих параметров и серверной проверки токенов. Секреты хранятся только в Cloudflare Pages secrets.

## Apple

Sign in with Apple отложен. Для веб-интеграции требуется активная Apple Developer Program. Схема базы уже допускает provider `apple`; повторно менять структуру при подключении не потребуется.

## Cloudflare D1

1. Создать D1 database с именем `cyd-satellite-tracker-auth`.
2. Вставить database ID вместо `REPLACE_WITH_D1_DATABASE_ID` в `wrangler.toml`.
3. Применить миграцию: `wrangler d1 migrations apply cyd-satellite-tracker-auth --remote`.
4. Убедиться, что Pages Functions проекта используют D1 binding с именем `DB`.

## API

- `POST /api/auth/register` — создать аккаунт (email, пароль от 12 символов, необязательное имя).
- `POST /api/auth/login` — войти.
- `POST /api/auth/logout` — завершить сессию.
- `GET /api/auth/me` — получить текущего пользователя.

## Безопасность и секреты

Не публикуйте пароли, OAuth client secrets, приватные ключи или реальные токены в GitHub, переписке или клиентском JavaScript. Не считайте регистрацию активной, пока D1 не создана, миграция не применена и production deployment не проверен.


## Администрирование пользователей
Добавлена серверная админ-панель с ролями, блокировками, удалением, сбросом сессий, списком устройств и аудитом. Инструкция ручного назначения первого администратора: ADMIN.md. Админ-панель доступна только после подключения D1 и назначения роли admin в базе.
