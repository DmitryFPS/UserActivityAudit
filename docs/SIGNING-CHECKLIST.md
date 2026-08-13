# Подпись бинарников UserActivityAudit (опционально)

**v1.0 standalone:** test-sign (minifilter) + WDAC/GPO для user-mode exe. EV **не обязателен**.

EV Code Signing — только при требовании заказчика или mass SmartScreen без GPO.

---

## Подготовка (если нужен EV)

1. EV-сертификат в `Cert:\CurrentUser\My` или PFX
2. Windows SDK — `signtool.exe`
3. Собранный пакет: `.\installer\build-dist.ps1`

## Автоматическая подпись

```powershell
.\installer\sign.ps1 -DistRoot dist\UserActivityAudit-1.0.0 `
  -Pfx C:\secure\UserAudit-ev.pfx -Password (Read-Host -AsSecureString)

# или thumbprint
.\installer\sign.ps1 -DistRoot dist\UserActivityAudit-1.0.0 -CertThumbprint ABCD...
```

Подписываются: `UserAudit.exe`, `UserAuditAdmin.exe`, `UserAuditWatchdog.exe`, `UserAuditKeygen.exe`, `UserAudit.Dashboard.exe`, `UserAuditSetup.msi`.

## Minifilter

| Этап | Инструмент |
|------|------------|
| v1.0 lab/prod | Self-signed + `testsigning on` + verify-driver |
| v1.1+ enterprise | WHQL/attestation |

## Чеклист v1.0 (без EV)

- [x] ARM1: reboot + soak + verify-driver PASS
- [x] deploy.ps1 / MSI
- [ ] WDAC policy для путей установки
- [ ] org.key у IT Security

## Чеклист EV (опционально)

- [ ] EV-подпись exe и MSI
- [ ] Timestamp (`http://timestamp.digicert.com`)
- [ ] WDAC по publisher
