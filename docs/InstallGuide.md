# Руководство по установке — UserActivityAudit 1.0

Автономный аудит на Windows 10/11 x64. **Сервер не требуется.**

---

## 1. Требования

| Компонент | Минимум |
|-----------|---------|
| ОС | Windows 10/11 x64 |
| ОЗУ | 2 GB (профиль Low) |
| Права | Локальный администратор для установки |
| .NET | 10 Desktop Runtime (для Dashboard) |
| BitLocker | Рекомендуется (политика организации) |

---

## 2. Быстрая установка (MSI)

```powershell
# От администратора
msiexec /i UserAuditSetup.msi /quiet /norestart
sc query UserAuditSvc
```

Ожидание: `STATE: RUNNING`, логи в `C:\ProgramData\UserAudit\logs\*.jsonl.enc`.

---

## 3. Установка через deploy.ps1

```powershell
.\deploy.ps1 -Profile auto -HostId NB-001
```

Скрипт: сборка/копирование → Program Files → config → служба → `--decrypt --verify` → SmokeImport.

---

## 4. Проверка после установки

```powershell
# Служба
sc query UserAuditSvc

# Логи (расшифровка)
& "$env:ProgramFiles\UserAudit\UserAudit.exe" --decrypt --verify

# Анализатор (тот же ПК, admin)
.\UserAudit.Dashboard.exe
```

---

## 5. После перезагрузки

```powershell
.\Tools\verify-reboot.ps1
```

Или вручную: `sc query UserAuditSvc` → RUNNING, новый `.enc` в logs.

---

## 6. IT USB (остановка / удаление)

```powershell
# Offline на IT-флешке
UserAuditKeygen.exe --out E:\IT\
copy E:\IT\org.pub C:\ProgramData\UserAudit\keys\org.pub

# Остановка
UserAuditAdmin.exe --sign-stop --key E:\IT\org.key
sc stop UserAuditSvc

# Удаление
UserAuditAdmin.exe --uninstall --key E:\IT\org.key
UserAudit.exe --uninstall
```

---

## 7. Minifilter

Входит в `deploy.ps1` (или `install-driver.ps1`). Требует **testsigning** в lab или WHQL в v1.1+.

```powershell
.\Tools\verify-driver.ps1
```

См. [DRIVER.md](DRIVER.md).

---

## 8. Удаление (dev без IT USB)

Только на тестовых машинах без `org.pub`:

```powershell
sc stop UserAuditSvc
sc delete UserAuditSvc
Remove-Item "$env:ProgramFiles\UserAudit" -Recurse -Force
```

В production удаление **только через IT USB**.
