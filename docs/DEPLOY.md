# Развёртывание (фаза 9)

Автономный сценарий — **без сервера**. Основной инструмент: [`installer/deploy.ps1`](../installer/deploy.ps1).

## Быстрый деплой одной машины

```powershell
# PowerShell от администратора, из корня репозитория
.\installer\deploy.ps1 -Profile auto
```

Скрипт:
1. Собирает native Release (или `-SkipBuild` для CI)
2. Копирует бинарники в `C:\Program Files\UserAudit\`
3. Создаёт `C:\ProgramData\UserAudit\config.json`
4. Регистрирует и запускает службу `UserAuditSvc`
5. Проверяет `--decrypt --verify` и `SmokeImport` (WPF Core)

Параметры:

| Параметр | По умолчанию | Описание |
|----------|--------------|----------|
| `-Profile` | `auto` | `low` / `standard` / `full` / `auto` |
| `-HostId` | имя ПК | Поле `host_id` в логах |
| `-IngestUrl` | `""` | Должен оставаться пустым (standalone, без upload) |
| `-SkipBuild` | — | Не пересобирать артефакты |
| `-SkipSmoke` | — | Пропустить SmokeImport |

## MSI (silent)

Каркас WiX: [`installer/wix/`](../installer/wix/README.md).

```powershell
msiexec /i build\UserAuditSetup.msi /quiet /norestart
```

## Анализатор (WPF)

На том же ПК, от администратора:

```powershell
dotnet run --project admin/UserAudit.Dashboard -c Release
```

Dashboard читает `%ProgramData%\UserAudit\logs` с `FileShare.ReadWrite` — работает **пока служба пишет логи**.

## GPO (Active Directory)

Развёртывание на эталоне ARM1 (1 ПК достаточно для v1.0) или fleet через GPO:

1. **GPO — служба:** запрет остановки `UserAuditSvc` для обычных пользователей.
2. **GPO — автозапуск:** `deploy.ps1` или MSI через startup script (SYSTEM).
3. **GPO — BitLocker:** рекомендуется на диске.
4. **IT USB:** `org.key` только у IT Security (`UserAuditKeygen`).

Шаблон startup script (GPO → Computer Configuration → Scripts → Startup):

```powershell
\\fileserver\share\UserActivityAudit\deploy.ps1 -Profile low -SkipBuild
```

## WDAC

v1.0: бинарники **unsigned**. Политика WDAC должна разрешать пути (Audit → Enforce):

- `C:\Program Files\UserAudit\*.exe`
- `UserAudit.Dashboard.exe` (если установлен в Program Files)

Шаблон: `installer/wdac/`. EV-подпись — опционально (v1.1+).

## Проверка после деплоя

См. [TESTING.md](TESTING.md) — чеклист фаз 1–4 и SmokeImport.
