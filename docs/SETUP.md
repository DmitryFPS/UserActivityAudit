# Сборка, установка и работа с логами

> **Автономный режим (без сервера):** см. [STANDALONE.md](STANDALONE.md) — основной сценарий развёртывания.

## Что изменилось в фазе 2 (шифрование)

**Раньше:** события писались в обычный текстовый файл `YYYY-MM-DD.jsonl` — его можно было открыть блокнотом.

**Сейчас:** каждая строка шифруется AES-256-GCM. На диске файл `YYYY-MM-DD.jsonl.enc` — **нечитаем без утилиты**.

| Что | Где лежит | Зачем |
|-----|-----------|-------|
| Зашифрованные логи | `C:\ProgramData\UserAudit\logs\` | Аудит-события по дням |
| Ключ шифрования (обёрнут DPAPI) | `C:\ProgramData\UserAudit\keys\master.key.dpapi` | Только этот ПК + права SYSTEM/admin |
| Состояние HMAC-цепочки | `C:\ProgramData\UserAudit\keys\chain.state` | Защита от подмены строк |

**Как прочитать логи:**

```powershell
# Admin PowerShell, из каталога с UserAudit.exe
.\UserAudit.exe --decrypt
.\UserAudit.exe --decrypt --date 2026-08-12 --verify
```

- `--decrypt` — расшифровка в stdout (обычный JSONL, по одной строке на событие)
- `--verify` — проверка HMAC-цепочки; код выхода `2` = обнаружена подмена
- `--date YYYY-MM-DD` — конкретный день (без параметра — сегодня по UTC)

**Важно:** расшифровка работает **на том же компьютере**, где писались логи (ключ привязан к машине через DPAPI). Перенос файла `.enc` на другой ПК без escrow (фаза 6) не даст прочитать содержимое.

---

## Один файл — `UserAudit.exe`

Для вас **один запуск**. Внутри программа сама поднимает нужные процессы:

| Режим | Как запускается | Кто видит |
|-------|-----------------|-----------|
| **Служба** | `UserAudit.exe --install` + `sc start` | Вы один раз |
| **User-agent** | Служба сама: `UserAudit.exe --user-agent --session-id N` | Автоматически при login |
| **Расшифровка** | `UserAudit.exe --decrypt` | Админ вручную |

Вам **не нужно** вручную запускать второй exe для сбора окон.

---

## 1. IntelliJ IDEA

IDEA не заменяет **MSVC**. Установите [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/) → **Desktop development with C++**.

Сборка из терминала IDEA:

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Debug
```

Подробнее — [BUILD.md](BUILD.md).

---

## 2. Установка (production)

```powershell
# Admin PowerShell
.\UserAudit.exe --install
sc start UserAuditSvc
```

Проверка, что логи пишутся (файл `.enc` появляется в каталоге):

```powershell
dir "C:\ProgramData\UserAudit\logs\"
```

Просмотр содержимого (расшифровка):

```powershell
.\UserAudit.exe --decrypt --date (Get-Date -Format yyyy-MM-dd) --verify
```

### Быстрый тест (Debug)

```powershell
.\UserAudit.exe
# Служба в dev-режиме сама запустит user-agent в вашей сессии
# Переключите окна → в расшифрованном выводе будет window.focus
```

### Security log (session.login и т.д.)

Нужна служба LocalSystem или admin. Если пусто — включите auditpol:

```powershell
auditpol /set /subcategory:"Logon" /success:enable /failure:enable
auditpol /set /subcategory:"Logoff" /success:enable
auditpol /set /subcategory:"Other Logon/Logoff Events" /success:enable
```

---

## 3. Каталоги и файлы

```
C:\ProgramData\UserAudit\logs\YYYY-MM-DD.jsonl.enc   ← зашифрованные события
C:\ProgramData\UserAudit\keys\master.key.dpapi       ← ключ (не открывать вручную)
C:\ProgramData\UserAudit\keys\chain.state            ← номер цепочки HMAC
```

При превышении 50 МБ за день создаётся дополнительный файл: `YYYY-MM-DD.1.jsonl.enc` и т.д.

---

## 6. Конфигурация (фаза 3)

Файл: `C:\ProgramData\UserAudit\config.json`

| Поле | Значение |
|------|----------|
| `profile` | `auto` / `low` / `standard` / `full` |
| `collectors.*` | Включение модулей (file, network, clipboard, print…) |
| `paths.critical` | Пути для FileCollector (`%USERPROFILE%\\Documents` и т.д.) |
| `storage.max_log_mb_per_day` | Лимит размера лога в день |

При `profile: auto` и ОЗУ ≤ 3 ГБ выбирается **Low** (реже опрос, меньше логов).

После изменения config — перезапуск службы:

```powershell
sc stop UserAuditSvc
sc start UserAuditSvc
```

Пример конфига — `installer/config.example.json`. Процедуры тестирования — [TESTING.md](TESTING.md).

---

## 7. Unit-тесты

```powershell
cmake --build build/native --config Debug --target test_event_serializer test_log_crypto test_hash_chain test_config_manager
.\build\native\tests\Debug\test_event_serializer.exe
.\build\native\tests\Debug\test_log_crypto.exe
.\build\native\tests\Debug\test_hash_chain.exe
.\build\native\tests\Debug\test_config_manager.exe
```

Или все сразу: `ctest --test-dir build/native -C Debug`

---

## 8. Удаление (только dev)

```powershell
.\UserAudit.exe --uninstall
```

В production удаление потребует IT USB (фаза 5).
