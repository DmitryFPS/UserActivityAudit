# MSI (фаза 9)

Каркас WiX для `UserAuditSetup.msi`. Полная сборка требует:

1. **WiX Toolset 3.11+** — https://wixtoolset.org/
2. Release-сборка native (`build/native/**/Release/*.exe`)
3. Копия `installer/config.example.json` в каталог дистрибутива

## Подготовка dist/

```powershell
$dist = "..\..\build\dist"
New-Item -Force -ItemType Directory $dist | Out-Null
Copy-Item ..\..\build\native\UserAuditSvc\Release\UserAudit.exe $dist
Copy-Item ..\..\build\native\UserAuditWatchdog\Release\UserAuditWatchdog.exe $dist
Copy-Item ..\..\build\native\UserAuditAdmin\Release\UserAuditAdmin.exe $dist
Copy-Item ..\config.example.json $dist
Copy-Item ..\deploy.ps1 $dist
```

## Сборка MSI

```powershell
cd installer\wix
candle -dSourceDir=..\..\build\dist Product.wxs
light -out ..\..\build\UserAuditSetup.msi Product.wixobj
```

## Silent install

```powershell
msiexec /i UserAuditSetup.msi /quiet /norestart
```

После установки MSI служба регистрируется через `ServiceInstall` в WiX. Для пилота рекомендуется **`installer/deploy.ps1`** — он выполняет smoke-проверку и настраивает `config.json`.

## GPO / WDAC

- GPO: см. `docs/DEPLOY.md` (шаблон политики — в следующем спринте)
- WDAC: политика должна разрешать подписанные бинарники из `Program Files\UserAudit`
