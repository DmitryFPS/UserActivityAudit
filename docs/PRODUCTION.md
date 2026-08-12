# Production — UserActivityAudit 1.0

## Сборка пилотного пакета

```powershell
# Admin, из корня репозитория
.\installer\build-dist.ps1
```

Результат:
- `dist\UserActivityAudit-1.0.0-rc1/` — полный пакет
- `dist\UserActivityAudit-1.0.0-rc1-win-x64.zip` — для раздачи

## Развёртывание одной машины

```powershell
msiexec /i Agent\UserAuditSetup.msi /quiet /norestart
# или
.\Agent\deploy.ps1 -SkipBuild -Profile low
```

## После reboot

```powershell
.\Tools\verify-reboot.ps1
```

## Подпись (обязательно для production)

```powershell
.\installer\sign.ps1 -DistRoot dist\UserActivityAudit-1.0.0-rc1 -Pfx ... 
```

См. [SIGNING-CHECKLIST.md](SIGNING-CHECKLIST.md).

## Документация

| Документ | Аудитория |
|----------|-----------|
| [InstallGuide.md](InstallGuide.md) | IT deploy |
| [AdminGuide.md](AdminGuide.md) | Админ / SOC |
| [SecurityModel.md](SecurityModel.md) | Security |
| [QA-PERFORMANCE.md](QA-PERFORMANCE.md) | QA sign-off |

## Осталось вручную (не автоматизируется)

1. **Reboot-тест** на эталонной машине  
2. **EV-подпись** dist  
3. **Minifilter** — [DRIVER-BUILD.md](DRIVER-BUILD.md) (опционально для пилота)  
4. **24h soak** — `Tools\soak-test.ps1` на каждой машине первую неделю
