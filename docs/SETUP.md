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

### Production (рекомендуется)

Запуск как **служба Windows** от **LocalSystem**:

```powershell
# PowerShell от имени администратора
cd C:\path\to\build\native\UserAuditSvc\Release
.\UserAuditSvc.exe --install
sc start UserAuditSvc
```

Проверка:

```powershell
sc query UserAuditSvc
Get-Content "C:\ProgramData\UserAudit\logs\$(Get-Date -Format yyyy-MM-dd).jsonl" -Tail 5
```

### Быстрый тест без службы

1. PowerShell **от имени администратора**
2. Запуск Debug-бинарника:

```powershell
.\build\native\UserAuditSvc\Debug\UserAuditSvc.exe
```

3. Выполните **Win+L** (блокировка) и войдите снова — в логе должны появиться `session.lock` / `session.login`.

### Если Security log пустой

Включите аудит входа (админ):

```powershell
auditpol /set /subcategory:"Logon" /success:enable /failure:enable
auditpol /set /subcategory:"Logoff" /success:enable
auditpol /set /subcategory:"Other Logon/Logoff Events" /success:enable
```

Увеличьте журнал Security: **gpedit.msc** → Конфигурация Windows → Параметры безопасности → Параметры событий → Security → **Max log size** ≥ 256 MB.

### ProcessCollector и ForegroundCollector

- **Process** (ETW): работает от обычного пользователя; от службы — стабильнее.
- **Foreground**: работает в сессии пользователя; из **LocalSystem** (служба без UI) **не видит** окна пользователя.

> **Phase 1 ограничение:** foreground из службы LocalSystem может не собирать окна. В Phase 3+ — collector в user-session или hybrid. Для теста `window.focus` запускайте **Debug exe от admin в интерактивной сессии**.

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
