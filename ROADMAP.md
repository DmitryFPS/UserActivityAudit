# UserActivityAudit — Дорожная карта разработки

Коммерческая версия 1.0. **Основной режим развёртывания:** автономный агент на каждом ПК + WPF-анализатор ([docs/STANDALONE.md](docs/STANDALONE.md)). Серверный стек (фаза 6) — опционально для парка машин.

Пилотный парк: 15 ноутбуков (поддержка 2 ГБ ОЗУ).

**Режим:** production-качество на каждой фазе. Без прототипных упрощений.

---

## Фаза 0 — Каркас ✅

**Цель:** структура репозитория, сборка, документация, каркас CI.

| Результат | Статус |
|-----------|--------|
| Структура каталогов (native/server/admin/installer) | ✅ |
| CMake + каркас UserAuditSvc | ✅ |
| ANALYTICS.md, ROADMAP.md, ARCHITECTURE.md | ✅ |
| Правило Cursor commercial-mode | ✅ |
| Заглушка docker-compose | ✅ |
| Git init | ✅ |
| Каркас GitHub Actions | ✅ |

### Критерии приёмки
- [x] `cmake -S native -B build/native` конфигурируется без ошибок (VS 2022)
- [x] UserAuditSvc компилируется (пустой цикл службы)
- [x] Документация для фазы 0 готова
- [x] `.cursor/rules/commercial-mode.mdc` на месте

---

## Фаза 1 — Сборщики L1 ✅

**Цель:** сбор в реальном времени — сессии, процессы, активное окно, USB.

| Модуль | Источник | Статус |
|--------|----------|--------|
| SessionCollector | Event Log 4624, 4634, 4800, 4801 | ✅ Спринт 1 |
| EventWriter (JSONL) | JSONL (до шифрования) | ✅ Спринт 1 |
| ProcessCollector | ETW Microsoft-Windows-Kernel-Process | ✅ Спринт 2 |
| ForegroundCollector | Тот же exe, `--user-agent` в сессии пользователя → pipe | ✅ Спринт 2b |
| UsbCollector | WMI Win32_VolumeChangeEvent | ✅ Спринт 3 |

### Критерии приёмки
- [ ] Служба работает как LocalSystem, переживает перезагрузку ([docs/TESTING.md](docs/TESTING.md)) — reboot не проверялся в этой сессии
- [x] Вход → событие `session.login` (SessionCollector)
- [x] notepad.exe → `process.start` + `window.focus` (через user-agent)
- [x] USB → `usb.insert` с VID/PID при наличии
- [x] Unit-тесты сериализации событий
- [x] Unit-тесты парсинга USB VID/PID
- [x] ОЗУ ≤ 20 МБ — main PID **11,5 МБ** на dev-машине ([docs/TESTING.md](docs/TESTING.md))

---

## Фаза 2 — Шифрование и хранение ✅

**Цель:** зашифрованные логи, целостность, основа управления ключами.

| Модуль | Технология |
|--------|------------|
| EncryptedLogWriter | BCrypt AES-256-GCM, потоковая запись |
| KeyManager | DPAPI (SYSTEM) + заглушка TPM seal |
| HashChain | HMAC-SHA256 на каждое событие |
| LogRotation | Файл по дням, лимит размера (50 МБ) |

### Критерии приёмки
- [x] Логи на диске не читаются без `--decrypt`
- [x] HMAC-цепочка проверяется; подмена обнаруживается (`--verify`, код 2) — `TamperVerifyTest` + unit `test_hash_chain`
- [x] Буфер записи ≤ 8 КБ на строку
- [x] CLI `--decrypt` в `UserAudit.exe`

**Что изменилось для администратора:** вместо `YYYY-MM-DD.jsonl` на диске лежат `YYYY-MM-DD.jsonl.enc`. Читать — только через `UserAudit.exe --decrypt`. Ключи — в `%ProgramData%\UserAudit\keys\`.

---

## Фаза 3 — L2 и профили ✅

**Цель:** модули с периодическим опросом + профили Low/Standard/Full.

| Модуль | Примечание |
|--------|------------|
| FileCollector | Уровни путей, съёмные диски, correlation с USB |
| NetworkCollector | GetExtendedTcpTable, 30–60 сек |
| ClipboardCollector | Только хеш, opt-in (user-agent) |
| PrintCollector | Operational log PrintService |
| ConfigManager | config.json + авто Low при ≤3 ГБ ОЗУ |

### Критерии приёмки
- [x] Профиль Low: ОЗУ ≤ 15 МБ — main PID **11,5 МБ**; CPU idle **0,000%** (60 с замер)
- [x] Создание файла на съёмном → событие с correlation
- [x] Снимок сети: PID + удалённый адрес
- [x] Смена профиля через config без пересборки (перезапуск службы)

---

## Фаза 4 — Базовая защита и выгрузка ✅

**Цель:** ACL, watchdog, детект tamper, клиент выгрузки на сервер.

| Модуль | Примечание |
|--------|------------|
| AclGuard | ACL ProgramData, периодическая самопроверка |
| Watchdog | `UserAuditWatchdog.exe` + SCM recovery |
| TamperCollector | Security/System events + локальный deny log |
| UploadClient | Пакетная выгрузка TLS (WinHTTP); mock → outbox |

### Критерии приёмки
- [x] Обычный пользователь не может удалить `%ProgramData%\UserAudit\` — DENY ACL + exit 1 при попытке delete logs
- [x] Попытка tamper → событие `tamper.*` (TamperCollector + AclGuard)
- [x] Watchdog + SCM recovery перезапускают службу за 60 сек
- [x] Upload отключён по умолчанию (`ingest_url: ""`); mock/HTTP — опционально

---

## Фаза 5 — Minifilter и IT USB ✅

**Цель:** защита на уровне ядра + криптографическая авторизация админа.

| Модуль | Примечание |
|--------|------------|
| UserAudit.sys | Minifilter: запрет delete/rename bin + logs ([docs/DRIVER.md](docs/DRIVER.md)) |
| IoctlGuard | stop/uninstall только с Ed25519 token (AuthGuard + pipe) |
| UserAuditAdmin | IT-инструмент: challenge-response, uninstall |
| UserAuditKeygen | Церемония генерации org key |
| Lockdown mode | Append-only при детекте tamper (DriverClient + LockdownManager) |

### Критерии приёмки
- [ ] Локальный admin не удаляет логи при загруженном драйвере ([docs/DRIVER.md](docs/DRIVER.md))
- [x] `sc stop` без валидной подписи IT USB — отказ (AuthGuard)
- [x] Церемония uninstall через UserAuditAdmin + `--uninstall`
- [x] Dev: инструкции test-signing в [docs/DRIVER.md](docs/DRIVER.md)

---

## Фаза 6 — Серверный стек ✅

**Цель:** ingest, escrow ключей, alerts, веб-портал.

| Сервис | Назначение | Статус |
|--------|------------|--------|
| UserAudit.Ingest | upload логов и событий | ✅ |
| UserAudit.Escrow | wrap/unwrap DEK, ротация | ✅ |
| UserAudit.Alerts | Движок правил, уведомления | ✅ |
| UserAudit.Portal | Список хостов, timeline, admin UI | ✅ |

Dev: HTTP + PostgreSQL через `docker compose up -d --build`. mTLS — hardening после пилота.

### Критерии приёмки
- [x] Docker compose поднимает все сервисы
- [ ] Upload агента → видно в портале за 5 мин (E2E на пилоте)
- [x] Escrow: DEK unwrap ролью admin (`X-UserAudit-Admin-Key`)
- [x] Alert на tamper-событие (агент → `POST /ingest/events`, AlertEngine)

---

## Фаза 7 — Админ-пакет (C#) ✅

**Цель:** WPF-дашборд, отчёты, расшифровка, forensic import.

| Модуль | Примечание | Статус |
|--------|------------|--------|
| LogImporter | Зашифрованный JSONL → domain models | ✅ |
| Dashboard | Timeline, хосты, alerts (WPF) | ✅ |
| Reports | Excel/PDF (ClosedXML + QuestPDF) | ✅ |
| DecryptTool | Escrow + локальный DEK | ✅ |

Запуск: `dotnet run --project admin/UserAudit.Dashboard`

### Критерии приёмки
- [x] Dashboard показывает события локального ПК (SmokeImport: **4021** событий, DPAPI) — E2E UI запуск OK
- [x] Отчёт Daily Activity: login, top apps, idle (estimate)
- [x] USB-отчёт с correlation ID
- [x] Экспорт forensic pack (ZIP)

---

## Фаза 8 — L3 Forensic ✅

**Цель:** глубокий сбор — браузер, Prefetch, реестр, evidence pack (триггер USB + опциональное расписание).

| Модуль | Примечание | Статус |
|--------|------------|--------|
| DeepCollector | Очередь, низкий приоритет потока, USB-триггер | ✅ |
| BrowserParser | Chrome, Edge, Firefox (winsqlite3) | ✅ |
| ArtifactParser | Prefetch manifest, UserAssist (registry) | ✅ |
| UsbRegistryParser | USBSTOR export (UsbForensicAudit-совместимо) | ✅ |
| EvidencePack | ZIP в `%ProgramData%\UserAudit\packs\` | ✅ |

### Критерии приёмки
- [x] L3 по расписанию/триггеру не блокирует L1 (отдельный поток, `THREAD_MODE_BACKGROUND`)
- [x] История/загрузки браузера в evidence pack (`browser/*.jsonl`)
- [x] USB registry + correlation в pack; UsbForensicAudit-формат export

**Конфиг:** `forensic.trigger_on_usb` (по умолчанию `true`), `schedule: off|weekly|nightly`

---

## Фаза 9 — Установщик и деплой ✅

**Цель:** MSI silent, GPO, WDAC policy, пилотный dist/.

| Результат | Статус |
|-----------|--------|
| UserAuditSetup.msi | ✅ |
| deploy.ps1, build-dist.ps1, sign.ps1 | ✅ |
| dist/ + ZIP | ✅ `dist/UserActivityAudit-1.0.0-rc1-win-x64.zip` |
| GPO / WDAC docs | ✅ |
| verify-reboot.ps1, soak-test.ps1 | ✅ |

### Критерии приёмки
- [x] Silent install (`msiexec /quiet`)
- [ ] Reboot — `verify-reboot.ps1` (выполняет администратор)
- [x] deploy.ps1 / build-dist
- [x] GPO задокументированы

---

## Фаза 10 — Release Candidate (RC1)

| Результат | Статус |
|-----------|--------|
| QA-PERFORMANCE.md | ✅ |
| InstallGuide, AdminGuide, SecurityModel (RU) | ✅ |
| SIGNING-CHECKLIST, RELEASE_NOTES | ✅ |
| PRODUCTION.md | ✅ |
| EV-подпись dist | ⏳ нужен сертификат |
| 24h soak | ⏳ скрипт готов, сбор CSV на пилоте |

### Критерии RC1 (автономный пилот)
- [x] Фазы 0–7, 9 (кроме reboot)
- [ ] Reboot + soak 24h на эталоне
- [x] Пилотный пакет в `dist/`
- [x] Release notes
- [ ] Minifilter .sys (опционально, см. DRIVER-BUILD.md)

---

## Рабочий процесс спринта

Каждая фаза — 2–4 спринта. После спринта:

1. Сборка + тесты проходят
2. Чекбоксы в ROADMAP обновлены
3. Краткий итог: сделано / дальше

**Текущая фаза:** **RC1 + L3 Forensic** (USB-триггер, evidence pack) → подпись EV + reboot-тест → rollout 15 ноутбуков
