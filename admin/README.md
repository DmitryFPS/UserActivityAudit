# UserAudit — Анализ логов (автономный режим)

WPF-приложение для просмотра **локальных** зашифрованных логов агента. Сервер не нужен.

## Сборка и запуск

```powershell
dotnet build admin/UserActivityAudit.Admin.slnx -c Release
dotnet run --project admin/UserAudit.Dashboard -c Release
```

**Требования:** Windows 10/11, .NET 10, **запуск от администратора** на том же ПК, где установлен агент.

## Возможности

| Функция | Описание |
|---------|----------|
| Автоимпорт | `%ProgramData%\UserAudit\logs\*.jsonl.enc` |
| Ключ DPAPI | `%ProgramData%\UserAudit\keys\master.key.dpapi` |
| Хронология | Все события с фильтрацией |
| Тревоги | tamper и critical из локального журнала |
| USB | События с correlation ID |
| Отчёты | Daily Activity и USB → Excel/PDF |
| Forensic pack | ZIP: events.jsonl + manifest.json |

## Структура

```
admin/
  UserAudit.Admin.Core/   — расшифровка, импорт, отчёты
  UserAudit.Dashboard/    — WPF UI (русский)
```

Полная инструкция: [docs/STANDALONE.md](../docs/STANDALONE.md).
