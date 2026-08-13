# QA Performance — UserActivityAudit

Дата замеров: 2026-08-12..13, эталон **ARM1**, Windows 10.

---

## Агент (Low profile, auto RAM)

| Метрика | Замер | Цель |
|---------|-------|------|
| RAM (main PID) | **14.7–15.1 MB** | ≤ 15 MB |
| CPU idle (60 s) | **0,000%** | ≤ 0,3% |

*User-agent — отдельный процесс в сессии пользователя.*

---

## Soak test (≥12 h)

Скрипты: `installer/soak-test.ps1`, `installer/verify-soak.ps1`  
Артефакт: `installer/results/soak-arm1-20260812.csv`

| Критерий | ARM1 |
|----------|------|
| Span | **13.0 h** (≥12 h) |
| service_state | RUNNING (31/31) |
| ram_mb_max | **15.08 MB** |
| decrypt_ok | **100%** (после warmup) |
| **Итог** | **PASS** |

Reboot: `verify-reboot.ps1` **PASS** (2026-08-13).

---

## Не блокирует RC1

| Пункт | Статус |
|-------|--------|
| Minifilter .sys на пилоте | опционально v1 (ACL + IT USB достаточно) |
| EV signature | перед mass rollout (`SIGNING-CHECKLIST.md`) |
| VM 2 GB matrix | по желанию |

---

## Rollout

1. ~~Эталон ARM1: reboot + soak ≥12 h~~ ✅  
2. EV-подпись `dist/`  
3. MSI/GPO на 15 ноутбуков (эталон = приёмка парка)
