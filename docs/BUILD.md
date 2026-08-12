# Сборка — нативный агент

См. также: [SETUP.md](SETUP.md) (IDEA, Security log, установка службы, расшифровка).

## Требования

- Windows 10/11 x64
- **Visual Studio Build Tools 2022** (или полная VS 2022) с компонентами:
  - Desktop development with C++
  - C++ CMake tools for Windows
- IntelliJ IDEA — опциональный редактор; **не заменяет** MSVC (см. SETUP.md)
- (Фаза 5+) Windows Driver Kit (WDK) — см. [DRIVER.md](DRIVER.md)

Результаты сборки:

| Бинарник | Путь |
|----------|------|
| UserAudit.exe | `build/native/UserAuditSvc/` |
| UserAuditWatchdog.exe | `build/native/UserAuditWatchdog/` |
| UserAuditKeygen.exe | `build/native/UserAuditKeygen/` |
| UserAuditAdmin.exe | `build/native/UserAuditAdmin/` |
| UserAuditFilter.sys | WDK/EWDK (см. DRIVER.md) |

## Конфигурация и сборка

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
cmake --build build/native --config Debug
```

Результат: `build/native/UserAuditSvc/UserAudit.exe` (один бинарник)

## Unit-тесты

```powershell
cmake --build build/native --config Debug --target test_event_serializer test_log_crypto test_hash_chain
ctest --test-dir build/native -C Debug
```

## Режим консоли (разработка)

В Debug-сборке определён `USERAUDIT_DEV_CONSOLE`. Запуск напрямую:

```powershell
.\build\native\UserAuditSvc\Debug\UserAudit.exe
```

Если процесс не запущен через SCM, агент работает в консольном режиме.

## Установка службы (admin, Release)

```powershell
.\build\native\UserAuditSvc\Release\UserAudit.exe --install
sc start UserAuditSvc
```

## Удаление службы (admin, только dev)

```powershell
.\build\native\UserAuditSvc\Release\UserAudit.exe --uninstall
```

> В production удаление требует церемонии с IT USB (фаза 5). Dev `--uninstall` — только для настройки.

## Расшифровка логов

```powershell
.\UserAudit.exe --decrypt --date 2026-08-12 --verify
```

Подробнее — в [SETUP.md](SETUP.md), раздел «Логи и шифрование».
