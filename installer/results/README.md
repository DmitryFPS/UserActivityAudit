# Soak test results (pilot)

| File | Machine | Span | Result | Date |
|------|---------|------|--------|------|
| [soak-arm1-20260812.csv](soak-arm1-20260812.csv) | ARM1 (эталон) | 13.0 h | **PASS** via `verify-soak.ps1` | 2026-08-12..13 |

**Критерий пилота:** ≥12 h на эталонной машине; успешный прогон на ARM1 принимается как валидация для rollout 15 ноутбуков.

Проверка:

```powershell
.\installer\verify-soak.ps1 -CsvPath .\installer\results\soak-arm1-20260812.csv
```
