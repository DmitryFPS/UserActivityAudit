# UserActivityAudit

Commercial-grade user activity audit system for Windows 10/11.

- **Client**: C++20 Windows Service (L1/L2/L3 collectors, encrypted logs, tamper protection)
- **Server**: .NET 10 — ingest, key escrow, alerts, web portal
- **Admin**: .NET 10 — WPF dashboard, Excel/PDF reports, forensic tools

Pilot deployment: 15 laptops (including 2 GB RAM machines).

## Documentation

| Document | Description |
|----------|-------------|
| [ANALYTICS.md](ANALYTICS.md) | Full product specification |
| [ROADMAP.md](ROADMAP.md) | Development phases 0–10 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | System architecture |
| [docs/BUILD.md](docs/BUILD.md) | Build instructions |
| [docs/SETUP.md](docs/SETUP.md) | IDEA, service, Security log |

## Build (Phase 0+)

### Native agent (Windows)

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
```

Output: `UserAudit.exe` (single binary — service + internal user-agent mode).

### Server & Admin (.NET 10)

Coming in Phase 6–7.

## License

Private / internal use. All rights reserved.
