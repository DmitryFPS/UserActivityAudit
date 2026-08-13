# Модель безопасности — UserActivityAudit 1.0

---

## 1. Угрозы и митигация

| Угроза | Митигация |
|--------|-----------|
| Чтение логов пользователем | AES-256-GCM, ключ DPAPI (SYSTEM/admin) |
| Подмена событий | HMAC-SHA256 цепочка, `--verify` |
| Удаление логов (user) | ACL DENY на ProgramData |
| Удаление логов (admin) | AclGuard + tamper + **minifilter** (kernel) |
| Остановка службы | AuthGuard + IT USB Ed25519 |
| Uninstall | UserAuditAdmin + org.key |
| Ключ на другом ПК | DPAPI привязка; экспорт DEK только admin |

---

## 2. Криптография

| Объект | Алгоритм |
|--------|----------|
| Логи на диске | AES-256-GCM (`v1:` + base64) |
| Цепочка | HMAC-SHA256 per event |
| Ключ DEK | 32 байта, обёртка DPAPI LocalMachine (`master.key.dpapi`) |
| IT USB | Ed25519 (org.key / org.pub) |

**v1.0:** DEK через DPAPI — штатный режим. TPM seal — v1.1+.

Plaintext на диске **не хранится**.

---

## 3. Разделение ролей

| Роль | Возможности |
|------|-------------|
| Пользователь | Нет доступа к логам/ключам |
| Локальный admin | Расшифровка, Dashboard; stop/uninstall **с IT USB** |
| IT Security | org.key на USB, Keygen, forensic |
| SYSTEM | Запись логов, DPAPI unwrap |

---

## 4. Tamper

- **AclGuard** — самопроверка ACL, событие `tamper.attempt_denied`
- **TamperCollector** — Security log
- **Lockdown** — append-only при детекте (с драйвером)
- Upload — **отключён** (`ingest_url: ""`)

---

## 5. Minifilter (UserAuditFilter.sys)

- Блок delete/rename в `\UserAudit\`
- Lockdown IOCTL — блок write
- **v1.0 lab/prod:** test-sign + `verify-driver.ps1 PASS`
- **v1.1+:** WHQL/attestation при требовании Secure Boot без testsigning

---

## 6. Сеть

По умолчанию **нет исходящих соединений** (`ingest_url: ""`).

---

## 7. Чеклист deploy (IT)

- [x] Эталон ARM1: soak + reboot + verify-driver PASS
- [ ] org.key только у IT Security
- [ ] WDAC / AppLocker — правила для `C:\Program Files\UserAudit\` (unsigned v1.0)
- [ ] testsigning=Yes — если нужен minifilter в lab
- [ ] Политика: keylog/screenshots **выключены** (opt-in)

EV-подпись **не требуется** для v1.0 standalone.

---

## 8. Forensic

- **Dashboard Forensic ZIP** — события L1/L2 из загруженного лога
- **Agent L3 pack** — `%ProgramData%\UserAudit\packs\*.zip`
- Browser history/downloads — Chrome, Edge, Firefox
- USBSTOR registry export — UsbForensicAudit-совместимо

См. [STANDALONE.md](STANDALONE.md), [DRIVER.md](DRIVER.md).
