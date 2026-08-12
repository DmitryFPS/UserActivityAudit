# Сборка minifilter UserAuditFilter.sys

## Проблема MSB8020

Ошибка `WindowsKernelModeDriver10.0 not found` означает: WDK headers установлены, но **интеграция с Visual Studio** — нет.

## Исправление (один раз на машине сборки)

1. Откройте **Visual Studio Installer**
2. Измените **Build Tools 2022**
3. Вкладка **Individual components** → отметьте:
   - **Windows Driver Kit**
   - **Windows 11 SDK (10.0.26100)** — если не установлен
4. Установите, перезапустите терминал

Или (admin PowerShell):

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify `
  --installPath "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools" `
  --add Microsoft.VisualStudio.ComponentGroup.WindowsDriverKit10 `
  --quiet --norestart
```

5. Проверка:

```powershell
Test-Path "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\km\fltkernel.h"
.\installer\build-driver.ps1
```

## Загрузка (lab)

```powershell
bcdedit /set testsigning on
# reboot
copy build\native\UserAuditFilter\Release\UserAuditFilter.sys C:\Windows\System32\drivers\
pnputil /add-driver native\UserAuditFilter\UserAuditFilter.inf /install
fltmc filters | findstr UserAudit
```

Production: WHQL — см. [SIGNING-CHECKLIST.md](SIGNING-CHECKLIST.md).

## Пилот без драйвера

Допустимо с принятием риска: ACL + AuthGuard + tamper events. Kernel-защита delete — после сборки `.sys`.
