# Сборка и запуск

## Один файл — `UserAudit.exe`

Для вас **один запуск**. Внутри программа сама поднимает нужные процессы:

| Режим | Как запускается | Кто видит |
|-------|-----------------|-----------|
| **Служба** | `UserAudit.exe --install` + `sc start` | Вы один раз |
| **User-agent** | Служба сама: `UserAudit.exe --user-agent --session-id N` | Автоматически при login |

Вам **не нужно** вручную запускать второй exe.

---

## 1. IntelliJ IDEA

IDEA не заменяет **MSVC**. Установите [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/) → **Desktop development with C++**.

Сборка из терминала IDEA:

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Debug
```

---

## 2. Установка (production)

```powershell
# Admin PowerShell
.\UserAudit.exe --install
sc start UserAuditSvc
```

Проверка логов:

```powershell
Get-Content "C:\ProgramData\UserAudit\logs\$(Get-Date -Format yyyy-MM-dd).jsonl" -Tail 10
```

### Быстрый тест (Debug)

```powershell
.\UserAudit.exe
# Служба в dev-режиме сама запустит user-agent в вашей сессии
# Переключите окна → window.focus в JSONL
```

### Security log (session.login и т.д.)

Нужна служба LocalSystem или admin. Если пусто — `auditpol` (см. ниже).

```powershell
auditpol /set /subcategory:"Logon" /success:enable /failure:enable
auditpol /set /subcategory:"Logoff" /success:enable
auditpol /set /subcategory:"Other Logon/Logoff Events" /success:enable
```

---

## 3. Логи

```
C:\ProgramData\UserAudit\logs\YYYY-MM-DD.jsonl
```

---

## 4. Unit-тесты

```powershell
cmake --build build/native --config Debug --target test_event_serializer
.\build\native\tests\Debug\test_event_serializer.exe
```

---

## 5. Удаление (dev)

```powershell
.\UserAudit.exe --uninstall
```
