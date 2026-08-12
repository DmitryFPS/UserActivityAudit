# Build Guide — Native Agent

## Prerequisites

- Windows 10/11 x64
- **Visual Studio 2022** with workloads:
  - Desktop development with C++
  - C++ CMake tools for Windows (includes CMake 3.x)
- (Phase 5+) Windows Driver Kit (WDK)

## Configure & build

```powershell
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
cmake --build build/native --config Debug
```

Output: `build/native/UserAuditSvc/Release/UserAuditSvc.exe`

## Development console mode

Debug builds define `USERAUDIT_DEV_CONSOLE`. Run directly:

```powershell
.\build\native\UserAuditSvc\Debug\UserAuditSvc.exe
```

When not started by Service Control Manager, the agent runs in console mode.

## Install service (admin, Release)

```powershell
.\build\native\UserAuditSvc\Release\UserAuditSvc.exe --install
sc start UserAuditSvc
```

## Uninstall service (admin)

```powershell
.\build\native\UserAuditSvc\Release\UserAuditSvc.exe --uninstall
```

> Production uninstall requires IT USB ceremony (Phase 5). Dev `--uninstall` is for setup only.
