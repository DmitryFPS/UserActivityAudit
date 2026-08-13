# Production — UserActivityAudit 1.0

## Сборка release-пакета

```powershell
# Admin, из корня репозитория
.\installer\build-dist.ps1
.\installer\verify-dist.ps1
```

Результат (версия из `VERSION`):
- `dist\UserActivityAudit-1.0.0/` — полный пакет
- `dist\UserActivityAudit-1.0.0-win-x64.zip` — для раздачи

## Развёртывание (один ПК — эталон ARM1)

```powershell
.\Agent\deploy.ps1 -SkipBuild -Profile low
# или
msiexec /i Agent\UserAuditSetup.msi /quiet /norestart
```

## Проверка после установки

```powershell
.\Tools\verify-reboot.ps1    # после перезагрузки
.\Tools\verify-driver.ps1    # minifilter + delete denied
.\Tools\verify-soak.ps1 -Csv installer\results\soak-arm1-20260812.csv
```

## Подпись (опционально)

Для v1.0 standalone достаточно **test-sign** (драйвер) + **WDAC/GPO** (user-mode). EV — только при требовании заказчика:

```powershell
.\installer\sign.ps1 -DistRoot dist\UserActivityAudit-1.0.0 -Pfx ...
```

См. [SIGNING-CHECKLIST.md](SIGNING-CHECKLIST.md).

## Документация

| Документ | Аудитория |
|----------|-----------|
| [InstallGuide.md](InstallGuide.md) | IT deploy |
| [AdminGuide.md](AdminGuide.md) | Админ / SOC |
| [SecurityModel.md](SecurityModel.md) | Security |
| [QA-PERFORMANCE.md](QA-PERFORMANCE.md) | QA sign-off |

## Приёмка v1.0 (ARM1 ✅ 2026-08-13)

| Критерий | Статус |
|----------|--------|
| Reboot | verify-reboot PASS |
| Soak ≥12 h | verify-soak PASS (13 h) |
| Minifilter | verify-driver PASS |
| dist + MSI | build-dist + verify-dist |
| Эталон | **1 ПК достаточно** для v1.0 |
