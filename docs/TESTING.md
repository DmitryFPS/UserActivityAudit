# Тестирование UserActivityAudit

Руководство по проверке критериев приёмки фаз 1–3 на Windows 10/11 x64.

---

## 1. Unit-тесты (обязательно перед деплоем)

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Debug
ctest --test-dir build/native -C Debug --output-on-failure
```

| Тест | Что проверяет |
|------|----------------|
| `test_event_serializer` | JSON-сериализация событий |
| `test_usb_helpers` | Парсинг VID/PID |
| `test_log_crypto` | AES-256-GCM roundtrip |
| `test_hash_chain` | HMAC-цепочка и детект подмены |
| `test_config_manager` | config.json, профили Low/Standard/Full |
| `test_upload_client` | Парсинг server/upload, mock vs HTTP |

---

## 2. Фаза 1 — служба и reboot

### 2.1 Установка

```powershell
# Admin
.\UserAudit.exe --install
sc start UserAuditSvc
sc query UserAuditSvc
```

Ожидание: `STATE: RUNNING`.

### 2.2 Reboot-тест

1. Перезагрузить ПК.
2. Через 2 минуты:

```powershell
sc query UserAuditSvc
dir C:\ProgramData\UserAudit\logs\
```

Ожидание: служба `RUNNING`, появился файл `YYYY-MM-DD.jsonl.enc`.

### 2.3 Ручные сценарии L1

| Действие | Ожидаемое событие (`--decrypt`) |
|----------|----------------------------------|
| Login / unlock | `session.login` / `session.unlock` |
| Запуск notepad.exe | `process.start` |
| Переключение окна | `window.focus` |
| USB-флешка | `usb.insert` с `vid`/`pid` |

```powershell
.\UserAudit.exe --decrypt --verify
```

---

## 3. Фаза 2 — шифрование

1. Открыть `C:\ProgramData\UserAudit\logs\*.jsonl.enc` в блокноте — **нечитаемый** текст (`v1:...`).
2. `UserAudit.exe --decrypt --verify` — JSONL в stdout, код выхода `0`.
3. Вручную изменить байт в `.enc` → `--verify` должен вернуть код `2`.

---

## 4. Фаза 3 — L2 и профили

### 4.1 Конфигурация

Файл: `C:\ProgramData\UserAudit\config.json` (создаётся автоматически при первом запуске).

```json
{
  "profile": "low",
  "collectors": {
    "file": true,
    "network": true,
    "clipboard": false,
    "print": true
  }
}
```

После изменения config.json — **перезапуск службы**:

```powershell
sc stop UserAuditSvc
sc start UserAuditSvc
```

### 4.2 File + correlation (USB)

1. Вставить USB-флешку → `usb.insert` с полем `corr`.
2. Создать файл на флешке → `file.create` с **тем же** `corr`.

### 4.3 Network snapshot

Подождать 30–60 сек (зависит от профиля). В логах:

```
"cat":"network","act":"snapshot"
"pid_0":"..."
"remote_0":"1.2.3.4:443"
```

### 4.4 Print (если включён)

Отправить документ на печать → событие `print.job`.

---

## 5. Замер RAM (критерий пилота)

На машине с 2 ГБ ОЗУ или VM:

```powershell
# PowerShell от admin, служба работает ≥5 мин
$proc = Get-Process UserAudit -ErrorAction SilentlyContinue
if ($proc) {
  "{0:N1} MB" -f ($proc.WorkingSet64 / 1MB)
} else {
  "Process not found — check service name/path"
}
```

**Цель фазы 3 (Low):** ≤ 15 МБ.  
**Промежуточная цель фазы 1:** ≤ 20 МБ.

Повторить 3 раза с интервалом 5 мин, записать максимум.

---

## 6. CPU idle (ориентир)

```powershell
Get-Counter '\Process(UserAudit)\% Processor Time' -SampleInterval 1 -MaxSamples 60 |
  Select-Object -ExpandProperty CounterSamples |
  Measure-Object -Property CookedValue -Average
```

**Цель Low:** ≤ 0,3% в idle (без активности пользователя).

---

## 7. Чеклист перед пилотом (15 ноутбуков)

- [ ] Unit-тесты: все PASS
- [ ] Reboot: служба поднимается сама
- [ ] `--decrypt --verify`: код 0
- [ ] USB → file.create с correlation
- [ ] network.snapshot с pid + remote
- [ ] RAM ≤ 15 МБ (Low) или ≤ 20 МБ (Standard)
- [ ] config.json меняется без пересборки (перезапуск службы)

---

## 8. Фаза 4 — защита, watchdog, upload

### 8.1 ACL (AclGuard)

Под обычным пользователем (не admin):

```powershell
Remove-Item -Recurse -Force C:\ProgramData\UserAudit\logs -ErrorAction SilentlyContinue
```

Ожидание: **Access denied**. Служба (LocalSystem) продолжает писать логи.

### 8.2 Watchdog

```powershell
# Admin — остановить службу вручную
sc stop UserAuditSvc

# Запустить watchdog (отдельное окно)
.\UserAuditWatchdog.exe
```

Ожидание: в течение **60 сек** служба снова `RUNNING` (watchdog или SCM recovery).

### 8.3 Tamper-события

После `sc stop UserAuditSvc` в логах (после перезапуска):

```powershell
.\UserAudit.exe --decrypt --verify | Select-String tamper
```

Ожидание: `"cat":"tamper"` (например `service_stop` или `attempt_denied`).

### 8.4 Upload (mock)

По умолчанию `config.json` содержит `"ingest_url": ".../mock"` — файлы копируются в:

```
C:\ProgramData\UserAudit\outbox\
```

Проверка:

```powershell
dir C:\ProgramData\UserAudit\outbox\
```

---

## 9. Фаза 5 — IT USB и minifilter

### 9.1 Генерация org key (offline)

```powershell
.\UserAuditKeygen.exe --out E:\IT\
copy E:\IT\org.pub C:\ProgramData\UserAudit\keys\org.pub
```

`org.key` — только на IT USB.

### 9.2 Остановка службы с IT USB

```powershell
.\UserAuditAdmin.exe --sign-stop --key E:\IT\org.key
sc stop UserAuditSvc
```

Без подписи: `sc stop` **отклоняется** (если `org.pub` развёрнут).

### 9.3 Uninstall

```powershell
.\UserAuditAdmin.exe --uninstall --key E:\IT\org.key
.\UserAudit.exe --uninstall
```

### 9.4 Minifilter

См. [DRIVER.md](DRIVER.md) — сборка `.sys`, test-signing, проверка delete логов.

---

## 10. Устранение проблем

| Симптом | Решение |
|---------|---------|
| Нет `session.*` | Служба LocalSystem + `auditpol` (см. SETUP.md) |
| Нет `window.focus` | User-agent не запущен — проверить login-сессию |
| `--decrypt` fails | Запуск от admin на том же ПК |
| Config не применился | `sc stop/start UserAuditSvc` |
