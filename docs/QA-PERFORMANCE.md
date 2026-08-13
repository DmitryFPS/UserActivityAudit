# QA Performance — UserActivityAudit v1.0

Дата замеров: 2026-08-12..13, эталон **ARM1**, Windows 10.

**Единственный объективный gate:** `.\installer\verify-release.ps1` — score ≥ 95% = PASS.  
Не доверять процентам в markdown без вывода этого скрипта.

---

## Агент (Low profile, auto RAM)

| Метрика | Замер | Цель | Статус |
|---------|-------|------|--------|
| RAM (main PID) | **14.7–15.1 MB** | ≤ 15 MB | ✅ |
| CPU idle (60 s) | **0,000%** | ≤ 0,3% | ✅ |

---

## Soak / reboot / driver (ARM1)

| Тест | Результат |
|------|-----------|
| soak ≥12 h | PASS — `installer/results/soak-arm1-20260812.csv` |
| reboot | PASS — 2026-08-13 |
| verify-driver | PASS — 2026-08-13 |

---

## Release gate

```powershell
.\installer\verify-release.ps1
# JSON: installer/results/verify-release-latest.json
```

| Check | Weight |
|-------|--------|
| ctest | 15 |
| SmokeImport | 10 |
| TamperVerifyTest | 10 |
| verify-dist | 10 |
| verify-soak | 15 |
| verify-driver | 15 |
| UserAuditSvc RUNNING | 5 |
| docs-sync | 10 |
| VERSION 1.0.0 | 5 |
| security-model | 5 |

**PASS:** сумма ≥ 95 из 100.

---

## CI (GitHub Actions)

`build-native.yml`: cmake Release/Debug, ctest, admin build, SmokeImport, TamperVerifyTest.

---

## v1.1+ (не блокирует v1.0)

EV Code Signing, WHQL minifilter, TPM seal DEK — опционально.
