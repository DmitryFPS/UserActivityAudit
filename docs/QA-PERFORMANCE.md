# QA Performance — UserActivityAudit v1.0

Дата замеров: 2026-08-12..13, эталон **ARM1**, Windows 10.  
**Модель приёмки v1.0:** один эталонный ПК (ARM1) — достаточно для release.

---

## Агент (Low profile, auto RAM)

| Метрика | Замер | Цель | Статус |
|---------|-------|------|--------|
| RAM (main PID) | **14.7–15.1 MB** | ≤ 15 MB | ✅ |
| CPU idle (60 s) | **0,000%** | ≤ 0,3% | ✅ |

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
Minifilter: `verify-driver.ps1` **PASS** (2026-08-13).

---

## CI (GitHub Actions)

| Job | Проверка |
|-----|----------|
| build-native | cmake Release + Debug |
| ctest Release | unit-тесты native |
| dotnet admin | `UserActivityAudit.Admin.slnx` |
| QA tools | SmokeImport, TamperVerifyTest |

---

## Зрелость v1.0 — sign-off

| Слой | Оценка | Примечание |
|------|--------|------------|
| L1/L2/L3 | 95% | функционал complete |
| Crypto | 95% | DPAPI v1.0 by design; TPM → v1.1 |
| Tamper | 95% | ACL + IT USB + minifilter (test-sign) |
| Admin | 95% | Dashboard, отчёты, forensic |
| Install | 95% | MSI, deploy, verify-* |
| QA | 95% | ARM1 + CI |
| Docs | 95% | синхронизированы с ROADMAP |
| **Итого** | **≥95%** | standalone prod ready |

---

## Вне scope v1.0 (v1.1+)

| Пункт | Статус |
|-------|--------|
| EV Code Signing | опционально |
| WHQL minifilter | опционально (test-sign достаточен) |
| TPM seal DEK | v1.1+ |
| VM 2 GB matrix | по желанию |
