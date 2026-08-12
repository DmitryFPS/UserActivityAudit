# Сборка и запуск

## 1. IntelliJ IDEA и этот проект

**IntelliJ IDEA** (Java/Kotlin) **не содержит компилятор C++** для Windows. Для `UserAuditSvc` нужен **MSVC** (Microsoft C++).

### Что установить (один раз)

1. **[Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/)** (бесплатно)
   - Workload: **Desktop development with C++**
   - Компоненты: **MSVC**, **Windows SDK**, **C++ CMake tools for Windows**
2. Это **не заменяет** IDEA — даёт `cl.exe`, `cmake`, SDK для сборки из терминала.

### Как работать из IDEA

1. Откройте папку `UserActivityAudit` в IDEA (как обычный проект).
2. **View → Tool Windows → Terminal**
3. Сборка:

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Debug
```

4. Редактирование C++ в IDEA работает; **CLion** (от JetBrains) удобнее для C++ (отладчик, CMake UI), но **не обязателен**.

> **Итог:** IDEA — редактор + терминал; **Build Tools 2022** — компилятор.

---

## 2. SessionCollector (Security log) — что сделать

`SessionCollector` читает канал **Security** (события входа/выхода/блокировки). Обычный пользователь **не имеет** доступа.

```powershell
cmake --build build/native --config Debug
# Output (same folder):
#   UserAuditSvc/Debug/UserAuditSvc.exe
#   UserAuditUser/Debug/UserAuditUser.exe
```

> **Важно:** `UserAuditUser.exe` должен лежать **рядом** с `UserAuditSvc.exe`.

### Production (рекомендуется)

```powershell
# PowerShell от имени администратора — оба exe в одной папке
.\UserAuditSvc.exe --install
sc start UserAuditSvc
```

Служба автоматически:
1. Запускает **UserAuditUser.exe** в сессии каждого вошедшего пользователя
2. Принимает `window.focus` через named pipe `\\.\pipe\UserAudit\events`
3. Пишет всё в `%ProgramData%\UserAudit\logs\`

### Быстрый тест (Debug)

```powershell
# Admin PowerShell, оба exe в Debug/
.\UserAuditSvc.exe
# Служба поднимет UserAuditUser в текущей сессии
# Переключите окна — в JSONL появится window.focus
```

### SessionCollector (Security log)

Требует **LocalSystem** (служба) или **admin** (dev). События `session.login` / `session.lock` и т.д.

### ProcessCollector

ETW — работает из службы без доп. настроек.

### Если Security log пустой

Включите аудит входа (админ):

```powershell
auditpol /set /subcategory:"Logon" /success:enable /failure:enable
auditpol /set /subcategory:"Logoff" /success:enable
auditpol /set /subcategory:"Other Logon/Logoff Events" /success:enable
```

Увеличьте журнал Security: **gpedit.msc** → … → **Max log size** ≥ 256 MB.

---

## 3. Каталог логов

Все события пишутся **только** в:

```
C:\ProgramData\UserAudit\logs\YYYY-MM-DD.jsonl
```

Переопределение через переменные окружения **отключено** (production policy).

---

## 4. Unit-тесты

```powershell
cmake --build build/native --config Debug --target test_event_serializer
.\build\native\tests\Debug\test_event_serializer.exe
```

---

## 5. Удаление службы (dev)

```powershell
.\UserAuditSvc.exe --uninstall
```

Production uninstall — IT USB (Phase 5).
