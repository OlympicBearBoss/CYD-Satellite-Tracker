# История выпусков прошивки CYD Satellite Tracker

Этот каталог — постоянный журнал прошивок. Для каждого выпуска здесь хранится отдельный файл изменений, связанный с конкретным Git-коммитом и сборкой GitHub Actions.

## Правила выпусков

1. **Один выпуск — один неизменяемый номер версии.** Номер нельзя повторно использовать для другого содержимого.
2. Перед сборкой фиксируются изменения в `docs/releases/<VERSION>.md`.
3. В changelog обязательно указываются дата, версия, полный SHA исходного коммита, изменения, исправления, ограничения и статус проверки.
4. GitHub Actions добавляет changelog, сведения о сборке и SHA-256 в комплект файлов.
5. Проверенная сборка публикуется отдельным GitHub Release по тегу `firmware-v...`. Release хранит бинарные файлы постоянно, в отличие от временных Actions artifacts.
6. Неудачные и непроверенные сборки не объявляются стабильными. Аппаратная проверка отмечается отдельно от успешной компиляции.
7. Существующие версии не переписываются. Исправление выпускается под новым номером.

## Текущая точка учёта

| Версия | Дата | Исходный коммит | Статус |
|---|---|---|---|
| V96 | 2026-11-17 | [f504bd6](https://github.com/OlympicBearBoss/CYD-Satellite-Tracker/commit/f504bd6c7358b35ab4a97a7624188aab8e2240b8) | Сборка CI прошла; аппаратная проверка не подтверждена; не считать стабильным релизом |

## Важные исторические точки интерфейса

- [V50 — instrument UI redesign](https://github.com/OlympicBearBoss/CYD-Satellite-Tracker/commit/d1dbb7a30f0ef0e3cd27ff73046629544f0ca59e)
- [V62 — four-zone instrument dashboard](https://github.com/OlympicBearBoss/CYD-Satellite-Tracker/commit/21aa669148ed94a591cf59886e37985ce6a1be61)
- [Minimal Blue HOME layout](https://github.com/OlympicBearBoss/CYD-Satellite-Tracker/commit/9ed81b5970c8b00c4d61fa1bf43c084cf51fc25d)
- [V91 — satellite image cache, touch and connection indicator fixes](https://github.com/OlympicBearBoss/CYD-Satellite-Tracker/commit/a3bfa728a0adfffb2117b7ff697dc5761d3c3fec)

Эти записи помогают найти историю изменений, но сами по себе не означают, что указанная версия является текущей рекомендуемой прошивкой.

## Как выпустить новую версию

1. Внести изменения в исходники и проверить diff относительно предыдущего выпуска.
2. Обновить `firmware/VERSION` и создать `docs/releases/<VERSION>.md` по шаблону `TEMPLATE.md`.
3. Проверить сборку на ветке и исправить все ошибки CI.
4. После проверки создать тег `firmware-<VERSION>`, например `firmware-v2026.11.18.97`.
5. GitHub Actions соберёт бинарные файлы, приложит changelog и SHA-256 и опубликует GitHub Release.
6. Аппаратную проверку и результат пользователя дописать в changelog отдельным последующим коммитом; исходный номер версии и бинарные файлы не заменять.

См. также [процесс выпуска прошивки](../../firmware/RELEASE_PROCESS.md).
