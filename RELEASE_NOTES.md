# Release Notes — UserActivityAudit 1.0.0

**Дата:** 2026-08-13  
**Режим:** автономный (без центрального сервера)

---

## Продукт

- **Агент** — служба Windows, сбор L1/L2/L3, AES-256-GCM, HMAC, tamper, ACL, minifilter
- **Dashboard** — WPF анализатор логов, отчёты Excel/PDF, forensic ZIP
- **Установка** — MSI silent, deploy.ps1, GPO-документация

---

## v1.0.0

- Standalone: `ingest_url` по умолчанию пустой
- LocalKeyProvider (DPAPI) для Dashboard
- LogImporter: чтение `.enc` при работающей службе (FileShare.ReadWrite)
- Пакет `dist/`: MSI + Dashboard + Tools + Docs + Driver
- **Minifilter:** UserAuditFilter.sys — build/install/verify pipeline
- QA: TamperVerifyTest, SmokeImport, soak/reboot/driver verify — **ARM1 PASS**
- L3 Forensic: USB-триггер → evidence pack
- WiX MSI x64, `msiexec /quiet`

---

## Приёмка

| Тест | ARM1 |
|------|------|
| soak ≥12 h | PASS (13 h) |
| reboot | PASS |
| verify-driver | PASS |
| dist | build-dist + verify-dist |

---

## Известные ограничения (v1.1+)

- Minifilter в lab: **test-signing** (`bcdedit /set testsigning on`)
- TPM seal DEK — v1.1 (v1.0: DPAPI LocalMachine)
- EV-подпись — опционально (`sign.ps1`)

---

## Обновление с dev-сборки

```powershell
sc stop UserAuditSvc
Get-Process UserAudit -ErrorAction SilentlyContinue | Stop-Process -Force
sc delete UserAuditSvc
msiexec /i UserAuditSetup.msi /quiet
```

Логи в `ProgramData\UserAudit` сохраняются (config ACL-protected).

---

## Документация

- [InstallGuide.md](docs/InstallGuide.md)
- [AdminGuide.md](docs/AdminGuide.md)
- [SecurityModel.md](docs/SecurityModel.md)
- [STANDALONE.md](docs/STANDALONE.md)
