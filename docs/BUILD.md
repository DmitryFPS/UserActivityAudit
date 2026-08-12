# Build Guide — Native Agent

See also: [SETUP.md](SETUP.md) (IntelliJ IDEA, Security log, service install).

## Prerequisites

- Windows 10/11 x64
- **Visual Studio Build Tools 2022** (or full VS 2022) with:
  - Desktop development with C++
  - C++ CMake tools for Windows
- IntelliJ IDEA — optional editor; **does not replace** MSVC (see SETUP.md)
- (Phase 5+) Windows Driver Kit (WDK)

## Configure & build

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
cmake --build build/native --config Debug
```

Output: `build/native/UserAuditSvc/UserAudit.exe` (single binary)

## Run unit tests

```powershell
cmake --build build/native --config Debug --target test_event_serializer
ctest --test-dir build/native -C Debug
```

## Development console mode

Debug builds define `USERAUDIT_DEV_CONSOLE`. Run directly:

```powershell
.\build\native\UserAuditSvc\Debug\UserAudit.exe
```

When not started by Service Control Manager, the agent runs in console mode.

## Install service (admin, Release)

```powershell
.\build\native\UserAuditSvc\Release\UserAudit.exe --install
sc start UserAuditSvc
```

## Uninstall service (admin)

```powershell
.\build\native\UserAuditSvc\Release\UserAudit.exe --uninstall
```

> Production uninstall requires IT USB ceremony (Phase 5). Dev `--uninstall` is for setup only.
