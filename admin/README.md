# Admin suite — Фаза 7

.NET 10 WPF: **Dashboard**, **LogImporter**, **Reports**, **Forensic pack**, **Escrow decrypt**.

## Сборка и запуск

```powershell
dotnet build admin/UserActivityAudit.Admin.slnx -c Release
dotnet run --project admin/UserAudit.Dashboard -c Release
```

## Возможности

| Модуль | Описание |
|--------|----------|
| **LogImporter** | Расшифровка `*.jsonl.enc` (AES-256-GCM, формат `v1:`) |
| **Dashboard** | Хосты, timeline, alerts с сервера (Portal/Ingest/Alerts API) |
| **Escrow decrypt** | Unwrap DEK по hostname через Escrow API + admin key |
| **Daily Activity** | Login count, top apps, focus estimate → Excel/PDF |
| **USB report** | События `usb.*` с correlation ID → Excel/PDF |
| **Forensic pack** | ZIP: `events.jsonl` + `manifest.json` |

## Подключение к серверу (dev)

| Поле | Значение по умолчанию |
|------|------------------------|
| Portal | http://127.0.0.1:8080 |
| Ingest | http://127.0.0.1:8081 |
| Escrow | http://127.0.0.1:8082 |
| Alerts | http://127.0.0.1:8083 |
| Admin key | `dev-admin-key-change-me` |

Локальные логи агента: `%ProgramData%\UserAudit\logs`

DEK: файл 32 байт, base64-текст, или unwrap через Escrow.

## Структура

```
admin/
  UserAudit.Admin.Core/   — crypto, import, reports, API clients
  UserAudit.Dashboard/    — WPF UI
```

Отчёты Excel/PDF — паттерн UsbForensicAudit (ClosedXML + QuestPDF).
