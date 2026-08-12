# QA и производительность — UserActivityAudit 1.0 RC1

Дата замеров: 2026-08-12, dev-машина ARM1, Windows 10.

---

## Unit-тесты (native)

| Тест | Результат |
|------|-----------|
| test_event_serializer | PASS |
| test_usb_helpers | PASS |
| test_log_crypto | PASS |
| test_hash_chain | PASS |
| test_config_manager | PASS |
| test_upload_client | PASS |
| test_ed25519_auth | PASS |

---

## E2E (автономный режим)

| Проверка | Результат |
|----------|-----------|
| Служба RUNNING | OK |
| `--decrypt --verify` | exit 0 |
| TamperVerifyTest → `--verify` | exit 2, restore → 0 |
| SmokeImport (DPAPI) | 4000+ events |
| MSI silent `/quiet` | OK, служба RUNNING |
| ACL delete logs (user) | Access denied |
| Dashboard launch | OK |

---

## RAM / CPU (Low profile)

| Метрика | Значение | Цель |
|---------|----------|------|
| Main service WS | **11,5 MB** | ≤ 15 MB |
| CPU idle (60 s) | **0,000%** | ≤ 0,3% |

*User-agent — отдельный процесс в сессии пользователя.*

---

## Soak test (24 h)

Скрипт: `installer/soak-test.ps1`  
Критерии: `svc=RUNNING`, `ram_mb_max` ≤ 15, `decrypt=ok` каждые 5 мин.

---

## Не проверено на этой машине

| Пункт | План |
|-------|------|
| Reboot | `verify-reboot.ps1` после перезагрузки |
| VM 2 GB | QA matrix Phase 10 |
| Minifilter loaded | test-signing + DRIVER.md |
| EV signature | SIGNING-CHECKLIST.md |

---

## Рекомендация для пилота

1. 1 эталонная машина: reboot + soak 24h  
2. Подписать dist EV-сертификатом  
3. Rollout 15 ноутбуков через GPO/MSI  
4. Сбор CSV soak с каждой машины за первую неделю
