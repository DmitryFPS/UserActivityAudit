# UserActivityAudit

Коммерческая система аудита действий пользователя для Windows 10/11.

- **Клиент**: служба Windows на C++20 (сборщики L1/L2/L3, шифрованные логи, защита от подмены)
- **Сервер**: .NET 10 — приём логов, escrow ключей, оповещения, веб-портал
- **Админка**: .NET 10 — WPF-дашборд, отчёты Excel/PDF, forensic-инструменты

Пилотное развёртывание: 15 ноутбуков (в том числе с 2 ГБ ОЗУ).

## Документация

| Документ | Описание |
|----------|----------|
| [ANALYTICS.md](ANALYTICS.md) | Полная спецификация продукта |
| [ROADMAP.md](ROADMAP.md) | Фазы разработки 0–10 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Архитектура системы |
| [docs/BUILD.md](docs/BUILD.md) | Сборка |
| [docs/SETUP.md](docs/SETUP.md) | Установка, служба, расшифровка логов |

## Сборка (фаза 0+)

### Агент (Windows)

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
```

Результат: `UserAudit.exe` — один файл (служба + внутренний user-agent).

### Сервер и админка (.NET 10)

Появятся в фазах 6–7.

## Лицензия

Частное / внутреннее использование. Все права защищены.
