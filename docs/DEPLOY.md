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

Рекомендуемый порядок пилота (15 ноутбуков):

1. **GPO — служба:** запрет остановки `UserAuditSvc` для обычных пользователей (настройки службы через GPO Security или SDDL).
2. **GPO — автозапуск:** `deploy.ps1` или MSI через startup script (SYSTEM).
3. **GPO — BitLocker:** напоминание включить шифрование диска (логи защищены DPAPI, но диск — базовый слой).
4. **Локальный admin:** выдать IT USB (`UserAuditKeygen` → `org.key` на флешке) только службе безопасности.

Шаблон startup script (GPO → Computer Configuration → Scripts → Startup):

```powershell
\\fileserver\share\UserActivityAudit\deploy.ps1 -Profile low -SkipBuild
```

## WDAC

Политика WDAC должна разрешать:
- `C:\Program Files\UserAudit\*.exe` (после EV-подписи в фазе 10)
- `UserAudit.Dashboard.exe` (если публикуется self-contained в `Program Files`)

До подписи — режим Audit Only на тестовой VM.

## Проверка после деплоя

См. [TESTING.md](TESTING.md) — чеклист фаз 1–4 и SmokeImport.
