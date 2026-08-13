# Руководство администратора — UserActivityAudit 1.0

---

## 1. Архитектура на одном ПК

| Компонент | Роль |
|-----------|------|
| `UserAudit.exe` (служба) | Сбор L1/L2, шифрование, tamper |
| `UserAudit.Dashboard` | Хронология, тревоги, отчёты, forensic ZIP |
| `UserAuditAdmin` + IT USB | Остановка/удаление (Ed25519) |
| `UserAuditWatchdog` | Перезапуск службы |

Данные: `C:\ProgramData\UserAudit\` (ACL, DPAPI-ключ).

---

## 2. Dashboard — ежедневная работа

1. Запуск **от администратора** на том же ПК, где работает агент
2. Автоимпорт `%ProgramData%\UserAudit\logs` каждые 60 сек
3. Вкладки:
   - **Обзор** — счётчики событий и тревог
   - **Хронология** — все события
   - **Тревоги** — tamper, ACL
   - **USB** — вставки с correlation ID
   - **Отчёты** — Excel/PDF по дню / USB
   - **Forensic** — ZIP-пакет для расследования

---

## 3. Конфигурация

Файл: `C:\ProgramData\UserAudit\config.json`

| Поле | Значение |
|------|----------|
| `profile` | `auto` / `low` / `standard` / `full` |
| `host_id` | Имя в логах (пилот: инв. номер) |
| `server.ingest_url` | Должен быть `""` (выгрузка отключена) |
| `collectors.*` | Включение модулей |

После изменения:

```powershell
sc stop UserAuditSvc
sc start UserAuditSvc
```

---

## 4. Расшифровка в консоли

```powershell
UserAudit.exe --decrypt --verify
UserAudit.exe --decrypt --date 2026-08-12 --verify
```

Код выхода `2` = подмена HMAC (расследование tamper).

---

## 5. Развёртывание парка (15 ноутбуков)

1. Распространить `dist\UserActivityAudit-*-win-x64.zip`
2. GPO Startup Script: `deploy.ps1 -Profile low -SkipBuild`
3. Или MSI: `msiexec /i UserAuditSetup.msi /quiet`
4. BitLocker + напоминание в GPO
5. IT USB: одна церемония `UserAuditKeygen`, `org.pub` на все ПК

Подробно: [DEPLOY.md](DEPLOY.md).

---

## 6. Soak / мониторинг (эталон ARM1)

```powershell
# ≥12 h на эталоне (ARM1 PASS — см. installer/results/)
.\installer\soak-test.ps1 -Hours 12 -IntervalSeconds 300
.\installer\verify-soak.ps1 -CsvPath .\installer\soak-YYYYMMDD-HHMMSS.csv
```

Критерии: span ≥12 h, `svc=RUNNING`, `ram_mb_max` ≤ 15, `decrypt=ok` ≥95%.

---

## 8. Эскалация

| Симптом | Действие |
|---------|----------|
| Служба STOPPED | Watchdog / SCM recovery; Event Log |
| `--verify` код 2 | Tamper — forensic pack, проверка ACL |
| Dashboard пустой | Запуск от admin; DPAPI только на том же ПК |
| ОЗУ > 15 MB | Профиль `low`, перезапуск службы |
