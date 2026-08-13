# QA — единственная метрика

```powershell
.\installer\verify-release.ps1
```

**PASS ≥ 95%** → `installer/results/verify-release-latest.json`

| Check | Pts |
|-------|-----|
| admin-build | 7 |
| ctest | 9 |
| SmokeImport | 7 |
| TamperVerify | 7 |
| verify-dist | 9 |
| verify-scripts | 5 |
| verify-soak | 10 |
| verify-driver | 10 |
| decrypt-verify | 10 |
| UserAuditSvc | 4 |
| docs | 7 |
| wdac-template | 4 |
| ci-workflow | 4 |
| version | 3 |
| security-model | 4 |
| **Итого** | **100** |

Никаких других % в репозитории. Canvas читает только JSON.
