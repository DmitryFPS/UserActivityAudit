# UserActivityAudit — Архитектура системы

Коммерческая версия 1.0 — **автономный режим**. Полная спецификация — в [ANALYTICS.md](../ANALYTICS.md).

---

## Общая схема

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    КЛИЕНТ + АНАЛИЗ (каждый ноутбук)                      │
│  ┌──────────────┐   ┌──────────────────────────────────────────────┐   │
│  │ UserAudit.sys│   │ UserAudit.exe (LocalSystem)                  │   │
│  │ (minifilter) │◄──│ L1/L2 + pipe; запускает --user-agent/сессию  │   │
│  └──────────────┘   └──────────────────────────────────────────────┘   │
│         │                              │                                 │
│         │         ┌────────────────────┘                                 │
│         │         ▼                                                      │
│         │    Зашифрованный JSONL (%ProgramData%\UserAudit\logs\)         │
│         │         │                                                      │
│  UserAuditWatchdog.exe (монитор + перезапуск)                           │
│         │         │                                                      │
│         │         ▼                                                      │
│  UserAudit.Dashboard (WPF) — хронология, alerts, отчёты, forensic       │
└─────────────────────────────────────────────────────────────────────────┘
```

Центрального сервера нет. Логи расшифровываются на том же ПК (DPAPI) или после экспорта DEK.

---

## Структура репозитория

```
UserActivityAudit/
├── native/                 # C++20 — агент и драйвер
│   ├── UserAuditSvc/       # Служба Windows + сборщики
│   ├── UserAuditWatchdog/
│   ├── UserAuditFilter/    # Minifilter ядра (фаза 5) → UserAudit.sys
│   ├── UserAuditAdmin/     # IT-инструмент авторизации
│   └── UserAuditKeygen/
├── admin/                  # .NET 10 — WPF и отчёты
│   ├── UserAudit.Dashboard/
│   ├── UserAudit.Reports/
│   ├── UserAudit.Forensic/
│   └── UserAudit.LogImporter/
├── installer/
├── tests/
└── docs/
```

---

## Уровни сбора

| Уровень | Частота | Примеры |
|---------|---------|---------|
| **L1** | Реальное время | Сессия, процесс ETW, активное окно, USB |
| **L2** | Периодически | Файлы, сеть, хеш буфера обмена, печать |
| **L3** | По расписанию / триггер | Браузер, Prefetch, реестр, evidence pack |

Профили (`Low` / `Standard` / `Full`) задают интервалы опроса и включённые модули.

---

## Конвейер событий (клиент)

```
Сборщики → Serializer → EncryptedLogWriter → Диск
  L1: session, process, usb, window (user-agent)
  L2: file, network, print (+ clipboard в user-agent, opt-in)
  ConfigManager: %ProgramData%\\UserAudit\\config.json
```

---

## Слои безопасности

| Слой | Компонент |
|------|-----------|
| Конфиденциальность | AES-256-GCM, DEK (DPAPI LocalMachine + опционально USB split) |
| Целостность | HMAC-цепочка на диске (`chain.state`) |
| Tamper (пользователь) | ACL + служба SYSTEM |
| Tamper (админ) | Minifilter + IT USB Ed25519 |
| Offline | BitLocker |

---

## Шифрование логов (фаза 2, реализовано)

1. Сборщик формирует JSON-событие (поля `id`, `ts`, `cat`, `act` и т.д.).
2. **HashChain** добавляет `seq`, `prev_hmac`, `hmac` — связь событий в цепочку.
3. **EncryptedLogWriter** шифрует строку AES-256-GCM (ключ DEK из **KeyManager**).
4. На диск пишется строка вида `v1:<base64>` в файл `YYYY-MM-DD.jsonl.enc`.

Ключи:

| Файл | Назначение |
|------|------------|
| `%ProgramData%\UserAudit\keys\master.key.dpapi` | DEK + HMAC key, обёрнуты DPAPI (LocalMachine) |
| `%ProgramData%\UserAudit\keys\chain.state` | Номер последнего события и последний HMAC |

Расшифровка на том же ПК (admin):

```powershell
UserAudit.exe --decrypt --date 2026-08-12 --verify
```

---

## Схема события (JSONL, до шифрования)

```json
{
  "id": "uuid-v7",
  "seq": 1001,
  "prev_hmac": "...",
  "hmac": "...",
  "ts": "2026-08-12T10:00:00.000Z",
  "lvl": 1,
  "cat": "session|process|window|file|network|usb|clipboard|print|registry|browser|alert|tamper",
  "act": "login|logout|start|stop|focus|create|insert|attempt_denied",
  "sev": "info|warning|critical",
  "host": "NB-01",
  "user": "DOMAIN\\user",
  "sid": "S-1-5-21-...",
  "sess": 1,
  "src": "etw|eventlog|wmi|driver",
  "corr": "uuid",
  "data": {}
}
```

На диске: зашифрованные blob AES-256-GCM (не plaintext).

---

## Стек технологий

| Слой | Стек |
|------|------|
| Агент | C++20, Win32, BCrypt (CNG), ETW, WMI |
| Драйвер | WDK, minifilter (FltMgr) |
| Админка | .NET 10, WPF |
| CI | GitHub Actions, MSVC 2022 |

---

## Целевые показатели (2 ГБ ОЗУ — профиль Low)

| Метрика | Цель |
|---------|------|
| ОЗУ агента | ≤ 15 МБ |
| CPU idle | ≤ 0,3% |
| Диск/день | ≤ 10 МБ |
| Задержка старта службы | 60–120 сек после boot |

---

## Внешние зависимости (v1.0 standalone)

| Зависимость | v1.0 | v1.1+ |
|-------------|------|-------|
| IT USB (org Ed25519) | **обязательно** | — |
| test-signing (minifilter lab) | **да** | WHQL вместо testsigning |
| WDAC / AppLocker для unsigned exe | **да** (GPO) | EV-подпись опционально |
| BitLocker | рекомендуется | — |
| Центральный сервер | **нет** | — |

---

## Интеграции

| Проект | Переиспользование |
|--------|-------------------|
| UsbForensicAudit | USB registry parsers, Excel/PDF, WlanApi |
| AutoConfigSec | Advanced Audit Policy, auditpol setup |
