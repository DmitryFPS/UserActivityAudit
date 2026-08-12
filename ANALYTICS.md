# UserActivityAudit — Аналитика продукта (Commercial v1.0)

Полная спецификация, собранная из сессий проектирования.  
**Режим:** коммерческий production. **Пилот:** 15 ноутбуков (поддержка 2 ГБ ОЗУ).

---

## 1. Кратко о продукте

| Параметр | Значение |
|----------|----------|
| Продукт | Аудит действий пользователя Windows 10/11 |
| Клиент | Служба C++20, ≤15 МБ ОЗУ (профиль Low) |
| Сервер | .NET 10 — ingest, escrow, alerts, веб-портал |
| Админка | .NET 10 — WPF, Excel/PDF, forensic |
| Безопасность | AES-256-GCM, split key TPM+USB, minifilter, IT USB |
| Деплой | MSI silent, GPO, Docker-сервер |

**УТП:** forensic-уровень аудита + лёгкий агент + защита от локального админа + шифрованные логи.

---

## 2. Цели и сценарии

- Корпоративный мониторинг (время, приложения, compliance)
- ИБ / расследование инцидентов (timeline, USB, exfiltration)
- Forensic evidence packs
- Compliance (152-ФЗ, GDPR, ISO 27001)

**Реалистичное покрытие:** ≥95% типичных действий пользователя с задокументированными пробелами.

---

## 3. Модель угроз

| Актор | Меры |
|-------|------|
| Обычный пользователь | ACL + служба SYSTEM + шифрованные логи |
| Локальный admin | Minifilter + IT USB + BitLocker |
| Offline-атака | BitLocker |
| Утечка файла лога | Split DEK (TPM ⊕ USB), escrow на сервере |
| Подмена лога | HMAC-цепочка + якорь на сервере |

---

## 4. Уровни сбора

### L1 — реальное время (всегда)
- Сессия: login/logout/lock/unlock/idle/RDP (4624, 4634, 4800, 4801)
- Процесс: ETW Kernel-Process, цепочка родителя, command line, SHA256 (выборочно)
- Активное окно: Application Sessions, опрос 3–5 сек
- USB: insert/remove, VID/PID/serial

### L2 — почти в реальном времени
- Файлы: уровни путей + съёмные + SACL чувствительных путей
- Сеть: TCP/UDP, DNS, bytes/PID (30–60 сек)
- Буфер обмена: только хеш (plaintext opt-in)
- Печать, privilege events, Wi-Fi SSID

### L3 — глубокий / forensic (расписание + триггер)
- История/загрузки браузера (Chrome, Edge, Firefox)
- Prefetch, Amcache, Jump Lists, UserAssist
- Реестр USB/Run/Tasks, cloud sync logs
- Evidence Pack ZIP

### Опциональные модули (выключены по умолчанию)
- Скриншоты по триггеру
- Keylog (opt-in, юридический gate)
- Запись экрана (только расследование)

---

## 5. Оповещения

| Правило | Серьёзность |
|---------|-------------|
| USB + копирование > 10 МБ | High |
| PowerShell -enc / запуск из TEMP | Critical |
| Массовое удаление > 50/мин | High |
| Tamper / попытка остановить службу | Critical |
| Процесс из denylist | Critical |
| Крупная загрузка аномальным PID | High |

---

## 6. Профили

| Параметр | Low (2 ГБ) | Standard | Full |
|----------|------------|----------|------|
| window_poll_sec | 5 | 3 | 2 |
| network_poll_sec | 60 | 30 | 15 |
| L3 schedule | weekly/trigger | nightly | nightly |
| screenshots | off | trigger | trigger |
| max_log_mb_day | 3 | 10 | 50 |

Автоопределение: ОЗУ ≤ 3072 → Low.

---

## 7. Архитектура

См. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
Клиент: UserAudit.sys + UserAuditSvc + Watchdog
Сервер: Ingest + Escrow + Alerts + Portal (Docker)
Админка: Dashboard + Reports + Forensic + UserAuditAdmin (IT USB)
```

---

## 8. Схема события (JSONL)

```json
{
  "id": "uuid-v7",
  "seq": 1001,
  "prev_hmac": "...",
  "hmac": "...",
  "ts": "2026-08-12T10:00:00.000Z",
  "lvl": 1,
  "cat": "session|process|window|file|network|usb|...",
  "act": "login|start|focus|create|insert|attempt_denied",
  "sev": "info|warning|critical",
  "host": "NB-01",
  "user": "DOMAIN\\user",
  "sid": "S-1-5-21-...",
  "corr": "uuid",
  "data": {}
}
```

На диске: AES-256-GCM (фаза 2 ✅). Plaintext JSONL больше не пишется.

---

## 9. Стек безопасности

### Шифрование
- AES-256-GCM потоково (BCrypt)
- DEK = K_machine (TPM-sealed) ⊕ K_usb (32 байта на IT USB)
- Escrow на сервере (RSA-OAEP wrap DEK)
- HMAC-цепочка на каждое событие

### Tamper
- L1: Служба LocalSystem, ACL Users=None
- L2: Watchdog, SCM recovery, tamper-события
- L3: GPO, AppLocker/WDAC
- L4: Minifilter — запрет delete/rename
- L5: BitLocker, удалённый backup

### IT USB
- Ed25519 challenge-response для uninstall/stop
- Церемония UserAuditKeygen при деплое
- Lockdown mode при детекте tamper

---

## 10. Производительность (цель 2 ГБ)

| Метрика | Цель |
|---------|------|
| ОЗУ | ≤ 15 МБ |
| CPU idle | ≤ 0,3% |
| Диск/день | ≤ 10 МБ |
| Задержка службы после boot | 60–120 сек |

---

## 11. Сервер (commercial)

| Сервис | Роль |
|--------|------|
| UserAudit.Ingest | mTLS upload, хранение |
| UserAudit.Escrow | Vault DEK, ротация |
| UserAudit.Alerts | Движок правил, notify |
| UserAudit.Portal | Хосты, timeline, admin |

---

## 12. Админ-пакет

- **Dashboard (WPF):** live timeline, 15 хостов, alerts
- **Reports:** Daily Activity, USB, Incident Timeline, Compliance
- **Forensic:** Evidence pack, интеграция UsbForensicAudit
- **UserAuditAdmin:** расшифровка, церемония uninstall

---

## 13. Юридические аспекты (RU)

- Уведомление сотрудников + политика мониторинга
- 152-ФЗ при ПДн в логах
- Keylog/скриншоты: отдельное согласие
- Хранение по умолчанию 90 дней

---

## 14. Внешние требования

- EV code signing
- Подпись драйвера Microsoft (HLK) для UserAudit.sys
- Сервер (VPS/on-prem)
- IT USB с org key
- BitLocker на пилотных ноутбуках

---

## 15. Интеграции

| Проект | Переиспользование |
|--------|-------------------|
| UsbForensicAudit | USB parsers, Excel/PDF, WlanApi |
| AutoConfigSec | auditpol, registry audit GPO prep |

---

## 16. Критерии приёмки (Commercial v1.0 RC)

1. L1+L2+L3 по профилю; ОЗУ ≤15 МБ на 2 ГБ
2. Admin не удаляет логи без IT USB (драйвер)
3. Утечка файла лога нечитаема без escrow
4. Портал показывает все пилотные хосты; alerts <60 сек
5. WPF + Excel/PDF + Forensic pack
6. MSI silent install + docs
7. 24ч soak test на VM 2 ГБ

---

## 17. Дорожная карта

См. [ROADMAP.md](ROADMAP.md) — фазы 0–10.

**Текущее состояние:** фаза 4 реализована (ACL, watchdog, tamper, upload mock) → **дальше:** ручной QA на VM + фаза 5 (minifilter).
