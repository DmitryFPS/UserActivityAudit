# UserActivityAudit — Дорожная карта разработки

Коммерческая версия 1.0. Пилотный парк: 15 ноутбуков.

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
- [ ] Служба работает как LocalSystem, переживает перезагрузку ([docs/TESTING.md](docs/TESTING.md))
- [x] Вход → событие `session.login` (SessionCollector)
- [x] notepad.exe → `process.start` + `window.focus` (через user-agent)
- [x] USB → `usb.insert` с VID/PID при наличии
- [x] Unit-тесты сериализации событий
- [x] Unit-тесты парсинга USB VID/PID
- [ ] ОЗУ ≤ 20 МБ ([docs/TESTING.md](docs/TESTING.md))

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
- [x] HMAC-цепочка проверяется; подмена обнаруживается (`--verify`, код 2)
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
- [ ] Профиль Low: ОЗУ ≤ 15 МБ, CPU ≤ 0,3% idle на VM 2 ГБ (процедура: [docs/TESTING.md](docs/TESTING.md))
- [x] Создание файла на съёмном → событие с correlation
- [x] Снимок сети: PID + удалённый адрес
- [x] Смена профиля через config без пересборки (перезапуск службы)

---

## Фаза 4 — Базовая защита и выгрузка

**Цель:** ACL, watchdog, детект tamper, клиент выгрузки на сервер.

| Модуль | Примечание |
|--------|------------|
| AclGuard | ACL ProgramData, периодическая самопроверка |
| Watchdog | Перезапуск службы при сбое |
| TamperCollector | Security events + локальный deny log |
| UploadClient | Пакетная выгрузка TLS 1.3 на ingest API |

### Критерии приёмки
- [ ] Обычный пользователь не может удалить `%ProgramData%\UserAudit\`
- [ ] Попытка tamper → событие `tamper.attempt_denied`
- [ ] Watchdog перезапускает службу за 60 сек
- [ ] Зашифрованные blob выгружаются на сервер (или mock в dev)

---

## Фаза 5 — Minifilter и IT USB

**Цель:** защита на уровне ядра + криптографическая авторизация админа.

| Модуль | Примечание |
|--------|------------|
| UserAudit.sys | Minifilter: запрет delete/rename bin + logs |
| IoctlGuard | uninstall/stop только с Ed25519 token |
| UserAuditAdmin | IT-инструмент: challenge-response, uninstall |
| UserAuditKeygen | Церемония генерации org key |
| Lockdown mode | Append-only при детекте tamper |

### Критерии приёмки
- [ ] Локальный admin не удаляет логи при загруженном драйвере
- [ ] `sc stop` без валидной подписи IT USB — отказ
- [ ] Церемония uninstall логируется на сервер до удаления
- [ ] Dev: инструкции test-signing задокументированы

---

## Фаза 6 — Серверный стек

**Цель:** ingest, escrow ключей, alerts, веб-портал.

| Сервис | Назначение |
|--------|------------|
| UserAudit.Ingest | mTLS upload, хранение |
| UserAudit.Escrow | wrap/unwrap DEK, ротация |
| UserAudit.Alerts | Движок правил, уведомления |
| UserAudit.Portal | Список хостов, timeline, admin UI |

### Критерии приёмки
- [ ] Docker compose поднимает все сервисы
- [ ] Upload агента → видно в портале за 5 мин
- [ ] Escrow: DEK восстанавливается ролью admin
- [ ] Alert на tamper-событие от агента

---

## Фаза 7 — Админ-пакет (C#)

**Цель:** WPF-дашборд, отчёты, расшифровка, forensic import.

| Модуль | Примечание |
|--------|------------|
| LogImporter | Зашифрованный JSONL → domain models |
| Dashboard | Timeline, хосты, alerts (WPF) |
| Reports | Excel/PDF (паттерны UsbForensicAudit) |
| DecryptTool | Escrow + локальный unwrap |

### Критерии приёмки
- [ ] Dashboard показывает все 15 пилотных хостов
- [ ] Отчёт Daily Activity: login, top apps, idle
- [ ] USB-отчёт с correlation ID
- [ ] Экспорт forensic pack (ZIP)

---

## Фаза 8 — L3 Forensic

**Цель:** глубокий сбор — браузер, Prefetch, реестр, evidence pack.

| Модуль | Примечание |
|--------|------------|
| DeepCollector | По расписанию + триггер (USB) |
| BrowserParser | Chrome, Edge, Firefox SQLite |
| ArtifactParser | Prefetch, Jump Lists, UserAssist |
| EvidencePack | ZIP: JSONL + артефакты |

### Критерии приёмки
- [ ] L3 по расписанию не блокирует L1 (отдельный поток, низкий приоритет)
- [ ] История браузера в evidence pack
- [ ] Интеграция с модулями UsbForensicAudit

---

## Фаза 9 — Установщик и деплой

**Цель:** MSI silent, GPO, WDAC policy.

| Результат | Примечание |
|-----------|------------|
| UserAuditSetup.msi | Silent `/quiet`, ACL, регистрация службы |
| deploy.ps1 | Имя машины, профиль, URL сервера |
| GPO templates | Защита службы, напоминание BitLocker |
| WDAC policy | Разрешить подписанные бинарники UserAudit |

### Критерии приёмки
- [ ] Silent install на чистой Win10/11 x64
- [ ] Автозапуск службы после reboot
- [ ] deploy.ps1 < 5 мин на машину
- [ ] GPO задокументированы для AD

---

## Фаза 10 — Release Candidate

**Цель:** hardening, QA, документация, чеклист подписи.

| Результат | Примечание |
|-----------|------------|
| QA matrix | VM 2 / 4 / 8 ГБ |
| Performance report | ОЗУ, CPU, диск/день |
| Security review | Tamper + crypto |
| Docs | InstallGuide, AdminGuide, SecurityModel (RU) |
| Signing checklist | EV cert, driver HLK |

### Критерии приёмки (Commercial v1.0 RC)
- [ ] Все критерии фаз 1–9 выполнены
- [ ] 24ч soak test на VM 2 ГБ: без падений, ОЗУ ≤ 15 МБ
- [ ] Пилотный пакет в `dist/`
- [ ] Release notes опубликованы

---

## Рабочий процесс спринта

Каждая фаза — 2–4 спринта. После спринта:

1. Сборка + тесты проходят
2. Чекбоксы в ROADMAP обновлены
3. Краткий итог: сделано / дальше

**Текущая фаза:** фаза 3 реализована (L2 + профили) → **Дальше:** добить замеры RAM/reboot на VM, затем **фаза 4** (по команде)
