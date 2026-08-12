# Подпись бинарников UserActivityAudit (production)

Используйте **EV Code Signing** сертификат. Driver `.sys` — отдельно через WHQL/attestation (см. [DRIVER.md](DRIVER.md)).

## Подготовка

1. EV-сертификат в хранилище `Cert:\CurrentUser\My` или PFX на защищённом носителе
2. [Windows SDK](https://developer.microsoft.com/windows/downloads/windows-sdk/) — `signtool.exe`
3. Собранный пакет: `.\installer\build-dist.ps1`

## Автоматическая подпись

```powershell
# Из PFX
.\installer\sign.ps1 -DistRoot dist\UserActivityAudit-1.0.0-rc1 `
  -Pfx C:\secure\UserAudit-ev.pfx -Password (Read-Host -AsSecureString)

# Из хранилища (thumbprint)
.\installer\sign.ps1 -DistRoot dist\UserActivityAudit-1.0.0-rc1 -CertThumbprint ABCD...
```

Подписываются: `UserAudit.exe`, `UserAuditAdmin.exe`, `UserAuditWatchdog.exe`, `UserAuditKeygen.exe`, `UserAudit.Dashboard.exe`, `UserAuditSetup.msi`.

## После подписи

1. Пересобрать ZIP: `Compress-Archive dist\UserActivityAudit-* ...`
2. Проверка: `signtool verify /pa Agent\UserAudit.exe`
3. SmartScreen: reputation нарастает после первых установок в организации

## Minifilter

| Этап | Инструмент |
|------|------------|
| Test lab | Self-signed + `bcdedit /set testsigning on` |
| Production | Microsoft Partner Center → attestation / HLK |

## Чеклист RC

- [ ] EV-подпись всех user-mode exe и MSI
- [ ] Timestamp (`http://timestamp.digicert.com`)
- [ ] Driver подписан (production) или test-signing (lab)
- [ ] WDAC policy обновлена под thumbprint/publisher
- [ ] Пилот: 1 машина → reboot → soak 24ч → 15 машин
