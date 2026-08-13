# UserAudit WDAC / AppLocker — v1.0 unsigned binaries

v1.0 ships **without Authenticode**. Deploy IT policy before Enforce.

## Paths to allow

```
C:\Program Files\UserAudit\UserAudit.exe
C:\Program Files\UserAudit\UserAuditAdmin.exe
C:\Program Files\UserAudit\UserAuditWatchdog.exe
C:\Program Files\UserAudit\*
```

Dashboard (optional separate install):

```
C:\Program Files\UserAudit\Admin\UserAudit.Dashboard.exe
```

## AppLocker (GPO) — audit first

```powershell
# Audit mode — Computer Configuration → Windows Settings → Security Settings
# → Application Control Policies → AppLocker → Executable Rules
# Create Path rule: C:\Program Files\UserAudit\*  Action: Allow
```

## WDAC supplemental (advanced)

1. Edit `UserAudit-supplemental.xml` paths if install dir differs.
2. Merge: `CiTool.exe -lp -d .` (see Microsoft WDAC docs).
3. Start with **Audit** on pilot PC, then Enforce.

## Verify

```powershell
.\installer\verify-release.ps1
# includes wdac-template check
```
