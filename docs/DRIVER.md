# Драйвер UserAuditFilter — сборка и test-signing

## Установка WDK

1. Visual Studio 2022 Build Tools / VS 2022 с «Desktop development with C++»
2. [Download the WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) — версия, совместимая с SDK 10.0.26100

Проверка:

```powershell
Test-Path "C:\Program Files (x86)\Windows Kits\10\Include\*\km\fltkernel.h"
```

## Исходники

| Файл | Назначение |
|------|------------|
| `native/UserAuditFilter/UserAuditFilter.c` | Minifilter callbacks |
| `native/UserAuditFilter/UserAuditFilter.inf` | INF для установки |
| `native/common/include/useraudit/driver_ioctl.hpp` | IOCTL + порт (shared) |

## Test-signing (лаборатория / VM)

```powershell
# Admin
bcdedit /set testsigning on
shutdown /r /t 0
```

Создайте test code signing cert (SelfSigned):

```powershell
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=UserAudit Test" -CertStoreLocation Cert:\CurrentUser\My
Export-PfxCertificate -Cert $cert -FilePath UserAuditTest.pfx -Password (ConvertTo-SecureString "test1234" -AsPlainText -Force)
```

Подпишите `UserAuditFilter.sys` (после сборки WDK):

```powershell
signtool sign /fd SHA256 /a /f UserAuditTest.pfx /p test1234 UserAuditFilter.sys
```

Установка:

```powershell
copy UserAuditFilter.sys C:\Windows\System32\drivers\
sc create UserAuditFilter type= filesys binPath= "C:\Windows\System32\drivers\UserAuditFilter.sys"
sc start UserAuditFilter
fltmc filters
```

## Production

- WHQL / Microsoft attestation signing
- EV code signing для user-mode (`UserAudit.exe`, `UserAuditAdmin.exe`)
- GPO + WDAC (фаза 9)

## Поведение

- Блок delete/rename файлов под `\UserAudit\`
- Lockdown (IOCTL) — блок write на защищённых путях
- Порт `\UserAuditFilter` — связь с `DriverClient` в службе

## Связка с IT USB

Остановка службы / uninstall без Ed25519-токена блокируется в user-mode (`AuthGuard` + `UserAuditAdmin`). Драйвер защищает **файлы** даже от локального admin.
