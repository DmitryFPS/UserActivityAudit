# UserActivityAudit — System Architecture

Commercial Production v1.0. See [ANALYTICS.md](../ANALYTICS.md) for full specification.

---

## High-level diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         CLIENT (each laptop)                             │
│  ┌──────────────┐   ┌──────────────────────────────────────────────┐   │
│  │ UserAudit.sys│   │ UserAudit.exe (LocalSystem)                  │   │
│  │ (minifilter) │◄──│ L1/L2 + pipe; spawns --user-agent per session │   │
│  └──────────────┘   └──────────────────────────────────────────────┘   │
│         │                              │                                 │
│         │         ┌────────────────────┘                                 │
│         │         ▼                                                      │
│         │    Encrypted JSONL (%ProgramData%\UserAudit\logs\)             │
│         │         │                                                      │
│  UserAuditWatchdog.exe (monitor + restart)                              │
└─────────────────┼───────────────────────────────────────────────────────┘
                  │ TLS 1.3 + mTLS (batch)
                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                              SERVER                                      │
│  Ingest API ──► Log Store    Escrow Vault ──► DEK wrap/unwrap           │
│       │              │              │                                    │
│       └──────► Alert Engine ◄───────┘                                    │
│                      │                                                   │
│               Web Portal (timeline, hosts, alerts)                       │
└─────────────────────────────────────────────────────────────────────────┘
                  ▲
                  │ decrypt / reports
┌─────────────────┴───────────────────────────────────────────────────────┐
│  ADMIN WORKSTATION (C#)                                                  │
│  UserAudit.Dashboard │ Reports │ Forensic │ UserAuditAdmin (IT USB)     │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Repository layout

```
UserActivityAudit/
├── native/                 # C++20 — client only
│   ├── UserAuditSvc/       # Windows Service + collectors
│   ├── UserAuditWatchdog/
│   ├── UserAuditFilter/      # Kernel minifilter (Phase 5) → builds UserAudit.sys
│   ├── UserAuditAdmin/     # IT authorization tool
│   └── UserAuditKeygen/
├── server/                 # .NET 10 — backend
│   ├── UserAudit.Ingest/
│   ├── UserAudit.Escrow/
│   ├── UserAudit.Alerts/
│   └── UserAudit.Portal/
├── admin/                  # .NET 10 — operator tools
│   ├── UserAudit.Dashboard/
│   ├── UserAudit.Reports/
│   ├── UserAudit.Forensic/
│   └── UserAudit.LogImporter/
├── installer/
├── tests/
└── docs/
```

---

## Collection tiers

| Tier | Frequency | Examples |
|------|-----------|----------|
| **L1** | Real-time | Session, process ETW, foreground, USB |
| **L2** | Periodic | Files, network, clipboard hash, print |
| **L3** | Scheduled / trigger | Browser, Prefetch, registry, evidence pack |

Profiles (`Low` / `Standard` / `Full`) control poll intervals and enabled modules.

---

## Event pipeline (client)

```
Collectors → EventQueue (ring buffer) → Serializer → CryptoWriter → Disk
                ↓
           AlertEngine (local rules) → UploadClient → Server
```

---

## Security layers

| Layer | Component |
|-------|-----------|
| Confidentiality | AES-256-GCM, DEK = TPM ⊕ USB, server escrow |
| Integrity | HMAC hash chain, server anchor |
| Tamper (user) | ACL + Service SYSTEM |
| Tamper (admin) | Minifilter + IT USB Ed25519 |
| Offline | BitLocker |
| Transport | TLS 1.3 + mTLS + cert pinning |

---

## Event schema (JSONL, pre-encryption)

```json
{
  "id": "uuid-v7",
  "seq": 1001,
  "prev_hmac": "...",
  "ts": "2026-08-12T10:00:00.000Z",
  "lvl": 1,
  "cat": "session|process|window|file|network|usb|clipboard|print|registry|browser|alert|tamper",
  "act": "login|logout|start|stop|focus|create|insert|attempt_denied",
  "sev": "info|warning|critical",
  "host": "NB-01",
  "user": "DOMAIN\\user",
  "sid": "S-1-5-21-...",
  "sess": 1,
  "src": "etw|eventlog|wmi|driver",
  "corr": "uuid",
  "data": {}
}
```

On disk: AES-256-GCM encrypted blobs (Phase 2+).

---

## Technology stack

| Layer | Stack |
|-------|-------|
| Client agent | C++20, Win32, BCrypt (CNG), ETW, WMI |
| Driver | WDK, minifilter (FltMgr) |
| Server | .NET 10, ASP.NET Core, Docker |
| Admin | .NET 10, WPF |
| CI | GitHub Actions, MSVC 2022 |

---

## Performance targets (2 GB RAM — Low profile)

| Metric | Target |
|--------|--------|
| Agent RAM | ≤ 15 MB |
| CPU idle | ≤ 0.3% |
| Disk/day | ≤ 10 MB |
| Service start delay | 60–120 sec after boot |

---

## External dependencies (pilot)

- EV code signing certificate (agent + admin)
- Microsoft driver signing (UserAudit.sys)
- Server host (VPS or on-prem) for Phase 6+
- IT USB with org Ed25519 key
- BitLocker on all pilot laptops

---

## Integration points

| Existing project | Reuse |
|------------------|-------|
| UsbForensicAudit | USB registry parsers, Excel/PDF, WlanApi |
| AutoConfigSec | Advanced Audit Policy, auditpol setup |
