# Модель безопасности — UserActivityAudit 1.0

---

## 1. Угрозы и митигация

| Угроза | Митигация |
|--------|-----------|
| Чтение логов пользователем | AES-256-GCM, ключ DPAPI (SYSTEM/admin) |
| Подмена событий | HMAC-SHA256 цепочка, `--verify` |
| Удаление логов (user) | ACL DENY на ProgramData |
| Удаление логов (admin) | AclGuard + tamper; **minifilter** (kernel) |
| Остановка службы | AuthGuard + IT USB Ed25519 |
| Uninstall | UserAuditAdmin + org.key |
| Ключ на другом ПК | DPAPI привязка; экспорт DEK только admin |

---

## 2. Криптография

| Объект | Алгоритм |
|--------|----------|
| Логи на диске | AES-256-GCM (`v1:` + base64) |
| Цепочка | HMAC-SHA256 per event |
| Ключ DEK | 32 байта, обёртка DPAPI (`master.key.dpapi`) |
| IT USB | Ed25519 (org.key / org.pub) |

Plaintext на диске **не хранится** (Phase 2+).

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
- **EventForwarder** — опционально на ingest API

---

## 5. Minifilter (UserAuditFilter.sys)

- Блок delete/rename в `\UserAudit\`
- Lockdown IOCTL — блок write
- Production: WHQL/attestation + EV user-mode

Без драйвера: защита user-mode ACL + AuthGuard (пилот допустим с принятием риска).

---

## 6. Сеть

По умолчанию **нет исходящих соединений** (`ingest_url: ""`).  
Upload — TLS (WinHTTP), опционально mTLS (server stack).

---

## 7. Соответствие пилоту (15 ноутбуков)

- [ ] BitLocker на дисках
- [ ] EV-подпись бинарников
- [ ] org.key только у IT Security
- [ ] WDAC / AppLocker — разрешить Publisher UserActivityAudit
- [ ] Политика: keylog/screenshots **выключены** (opt-in)

---

## 8. Forensic

Экспорт ZIP из Dashboard: расшифрованные события + метаданные.  
Не включает L3 browser/prefetch до Phase 8.

См. также [STANDALONE.md](STANDALONE.md), [DRIVER.md](DRIVER.md).
