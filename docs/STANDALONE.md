# UserActivityAudit — автономный режим (без сервера)

Продукт для **одного ПК**: агент собирает и шифрует логи локально, **UserAudit — Анализ логов** (WPF) читает их на том же компьютере.

Центральный сервер, Docker и PostgreSQL **не нужны**.

---

## 1. Установка агента

```powershell
# PowerShell от администратора
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release

copy installer\config.example.json C:\ProgramData\UserAudit\config.json
build\native\UserAuditSvc\Release\UserAudit.exe --install
sc start UserAuditSvc
```

Агент пишет в:
- Логи: `C:\ProgramData\UserAudit\logs\*.jsonl.enc`
- Ключ: `C:\ProgramData\UserAudit\keys\master.key.dpapi` (DPAPI, только этот ПК)

В `config.json` поле `"ingest_url": ""` — **выгрузка на сервер отключена**.

---

## 2. Анализ логов (WPF)

```powershell
dotnet build admin/UserActivityAudit.Admin.slnx -c Release
dotnet run --project admin/UserAudit.Dashboard -c Release
```

**Запускайте от администратора** на том же ПК, где работает агент — иначе DPAPI-ключ не расшифруется.

Приложение:
- автоматически читает `%ProgramData%\UserAudit\logs`
- обновляет данные каждые 60 секунд
- вкладки: обзор, хронология, тревоги (tamper), USB, отчёты Excel/PDF, forensic ZIP

Альтернатива в консоли:

```powershell
.\UserAudit.exe --decrypt --date 2026-08-12 --verify
```

---

## 3. Что входит в продукт

| Компонент | Назначение |
|-----------|------------|
| `UserAudit.exe` | Служба сбора L1/L2, шифрование, tamper, minifilter |
| `UserAuditWatchdog.exe` | Перезапуск службы |
| `UserAudit.Dashboard` | WPF — анализ, отчёты, forensic |
| `UserAuditAdmin.exe` | IT USB — остановка/удаление (фаза 5) |

---

## 4. Перенос логов на другой ПК

Файлы `.enc` можно копировать, но ключ DPAPI привязан к исходному ПК. На другой машине нужен **экспорт DEK** (файл 32 байта) или расшифровка через `UserAudit.exe --decrypt` на исходном ПК с pipe в файл.
