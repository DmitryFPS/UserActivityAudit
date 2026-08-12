# UserAuditFilter (UserAudit.sys)

Minifilter защищает `%ProgramData%\\UserAudit\\` от delete/rename; в lockdown блокирует write.

## Требования

- Visual Studio 2022 + **Windows Driver Kit (WDK) 10**
- Test-signing (dev) или Microsoft attestation signing (production)

## Сборка (Visual Studio + WDK)

1. Установите WDK: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
2. Откройте **Developer Command Prompt for VS 2022**
3. Соберите через MSBuild (после генерации проекта WDK) или EWDK:

```powershell
# Пример: msbuild после добавления .vcxproj WDK Driver
# Или см. docs/DRIVER.md
```

CMake-таргет `UserAuditFilter` включается только при `USERAUDIT_BUILD_DRIVER=ON` и наличии WDK.

## Test-signing (dev)

```powershell
bcdedit /set testsigning on
# reboot
```

Подпишите `.sys` test-сертификатом и установите:

```powershell
fltmc load UserAuditFilter
# или pnputil /add-driver UserAuditFilter.inf /install
```

## Проверка

```powershell
fltmc filters | findstr UserAudit
```

Под admin попытка удалить лог:

```powershell
del C:\ProgramData\UserAudit\logs\*.enc
```

Ожидание: **Access denied** при загруженном драйвере.

## Порт

User-mode: `FilterConnectCommunicationPort(L"\\UserAuditFilter", ...)`

См. `native/common/include/useraudit/driver_ioctl.hpp`.
