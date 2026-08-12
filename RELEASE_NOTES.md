# Release Notes — UserActivityAudit 1.0.0-rc1

**Дата:** 2026-08-12  
**Режим:** автономный (без центрального сервера)

---

## Продукт

- **Агент** — служба Windows, сбор L1/L2, AES-256-GCM, HMAC, tamper, ACL
- **Dashboard** — WPF анализатор логов, отчёты Excel/PDF, forensic ZIP
- **Установка** — MSI silent, deploy.ps1, GPO-документация

---

## Новое в RC1

- Standalone: `ingest_url` по умолчанию пустой
- LocalKeyProvider (DPAPI) для Dashboard
- LogImporter: чтение `.enc` при работающей службе (FileShare.ReadWrite)
- Пакет `dist/`: MSI + Dashboard + Tools + Docs
- QA: TamperVerifyTest, SmokeImport, soak-test.ps1
- **L3 Forensic:** USB-триггер → evidence pack (browser, USBSTOR, prefetch, UserAssist)
- WiX MSI x64, проверен `msiexec /quiet`

---

## Известные ограничения

- Minifilter `.sys` — исходник готов; сборка требует WDK toolset в VS
- Reboot-тест — выполняется администратором (`verify-reboot.ps1`)
- Подпись — unsigned до применения EV (`sign.ps1`)
- Серверный portal — опциональный модуль

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
