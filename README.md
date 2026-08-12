# UserActivityAudit

Аудит действий пользователя на Windows 10/11 — **автономный продукт без центрального сервера**.

| Компонент | Описание |
|-----------|----------|
| **Агент** (`UserAudit.exe`) | Служба на каждом ПК: сбор L1/L2, шифрование AES-256-GCM, tamper, minifilter |
| **Анализатор** (`UserAudit.Dashboard`) | WPF: хронология, тревоги, USB, отчёты Excel/PDF, forensic ZIP |

Docker и сервер **не требуются**. Подробно: [docs/STANDALONE.md](docs/STANDALONE.md).

## Быстрый старт

### Production-пакет (IT)

```powershell
.\installer\build-dist.ps1
# dist\UserActivityAudit-*-win-x64.zip → msiexec /quiet
```

См. [docs/PRODUCTION.md](docs/PRODUCTION.md).

### Агент (администратор)

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
.\build\native\UserAuditSvc\Release\UserAudit.exe --install
sc start UserAuditSvc
```

### Анализатор (администратор, тот же ПК)

```powershell
dotnet run --project admin/UserAudit.Dashboard -c Release
```

## Документация

| Документ | Описание |
|----------|----------|
| [docs/InstallGuide.md](docs/InstallGuide.md) | Установка (production) |
| [docs/AdminGuide.md](docs/AdminGuide.md) | Администрирование, Dashboard |
| [docs/SecurityModel.md](docs/SecurityModel.md) | Модель безопасности |
| [docs/STANDALONE.md](docs/STANDALONE.md) | **Основной режим** — без сервера |
| [docs/DEPLOY.md](docs/DEPLOY.md) | deploy.ps1, MSI, GPO |
| [docs/SETUP.md](docs/SETUP.md) | Установка, `--decrypt`, служба |
| [docs/BUILD.md](docs/BUILD.md) | Сборка native |
| [ANALYTICS.md](ANALYTICS.md) | Полная спецификация (включая опциональный server) |
| [ROADMAP.md](ROADMAP.md) | Фазы разработки |

Опциональный серверный стек (Ingest/Portal): [server/README.md](server/README.md).

## Лицензия

Частное / внутреннее использование.
