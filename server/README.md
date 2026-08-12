# Серверный стек UserActivityAudit (Фаза 6)

.NET 10: **Ingest**, **Escrow**, **Alerts**, **Portal** + PostgreSQL.

## Быстрый старт (Docker)

```powershell
docker compose up -d --build
```

| Сервис | URL |
|--------|-----|
| Portal | http://localhost:8080 |
| Ingest | http://localhost:8081 |
| Escrow | http://localhost:8082 |
| Alerts | http://localhost:8083 |

Admin API key (dev): заголовок `X-UserAudit-Admin-Key: dev-admin-key-change-me`

## Локальная разработка

1. PostgreSQL на `localhost:5432` (или только `docker compose up postgres -d`).
2. Сборка:

```powershell
dotnet build server/UserActivityAudit.Server.slnx
dotnet run --project server/UserAudit.Ingest --urls http://127.0.0.1:8081
dotnet run --project server/UserAudit.Portal --urls http://127.0.0.1:8080
```

БД создаётся автоматически (`EnsureCreated`) при первом запуске.

## API агента

- `POST /api/v1/ingest/logs` — тело `application/octet-stream`, заголовки `X-Log-File`, опционально `X-Host-Id`
- `POST /api/v1/ingest/events` — JSON tamper/audit-события (`IngestEventDto`)
- `GET /api/v1/escrow/public-key` — PEM RSA-4096 для wrap DEK
- `POST /api/v1/escrow/unwrap` — admin unwrap (заголовок admin key)

Базовый URL в `config.json` агента: `"ingest_url": "http://127.0.0.1:8081"`

## Структура

```
server/
  UserAudit.Shared/   — EF Core, сущности, AlertEngine, EscrowVault
  UserAudit.Ingest/   — upload логов и событий
  UserAudit.Escrow/   — escrow DEK
  UserAudit.Alerts/   — опрос событий, API alerts
  UserAudit.Portal/   — Razor UI: hosts, timeline, alerts
  docker/Dockerfile   — multi-stage build (ARG PROJECT)
```
