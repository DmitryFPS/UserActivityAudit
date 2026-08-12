# UserActivityAudit — Development Roadmap

Commercial Production v1.0. Pilot fleet: 15 laptops.

**Mode:** Production quality at every phase. No prototype shortcuts.

---

## Phase 0 — Scaffold ✅

**Goal:** Repository structure, build system, documentation, CI skeleton.

| Deliverable | Status |
|-------------|--------|
| Directory layout (native/server/admin/installer) | ✅ |
| CMake root + UserAuditSvc skeleton | ✅ |
| ANALYTICS.md, ROADMAP.md, ARCHITECTURE.md | ✅ |
| Cursor commercial-mode rule | ✅ |
| docker-compose placeholder | ✅ |
| Git init | ✅ |
| GitHub Actions build skeleton | ✅ |

### Acceptance criteria
- [x] `cmake -S native -B build/native` configures without error (VS 2022)
- [x] UserAuditSvc compiles (empty service loop)
- [x] Documentation complete for Phase 0
- [x] `.cursor/rules/commercial-mode.mdc` present

---

## Phase 1 — L1 Collectors (in progress)

**Goal:** Real-time collection — session, process, foreground window, USB.

| Module | Source | Status |
|--------|--------|--------|
| SessionCollector | Event Log 4624, 4634, 4800, 4801 | ✅ Sprint 1 |
| EventWriter (JSONL) | Plaintext JSONL | ✅ Sprint 1 |
| ProcessCollector | ETW Microsoft-Windows-Kernel-Process | ✅ Sprint 2 |
| ForegroundCollector | GetForegroundWindow, poll 5 sec | ✅ Sprint 2 |
| UsbCollector | WMI Win32_VolumeChangeEvent | Sprint 3 |

### Acceptance criteria
- [ ] Service runs as LocalSystem, survives reboot
- [x] Login event → JSONL `session.login` (SessionCollector)
- [x] notepad.exe → `process.start` + `window.focus` (dev console / user session)
- [ ] USB insert → `usb.insert`
- [x] Unit tests for event serialization
- [ ] RAM ≤ 20 MB (pre-crypto)

---

## Phase 2 — Crypto & Storage

**Goal:** Encrypted logs, integrity, key management foundation.

| Module | Technology |
|--------|------------|
| CryptoWriter | BCrypt AES-256-GCM streaming |
| KeyManager | DPAPI (SYSTEM) + TPM seal stub |
| HashChain | HMAC-SHA256 per event |
| LogRotation | Daily file, max size per profile |

### Acceptance criteria
- [ ] Log files not readable without decrypt tool
- [ ] Hash chain validates; tamper detected
- [ ] Streaming write RAM ≤ 8 KB buffer
- [ ] Decrypt CLI tool (native or admin preview)

---

## Phase 3 — L2 & Profiles

**Goal:** Near-real-time modules + Low/Standard/Full profiles.

| Module | Notes |
|--------|-------|
| FileCollector | Tiered paths, removable drives |
| NetworkCollector | GetExtendedTcpTable, 30–60 sec |
| ClipboardCollector | Hash only, opt-in |
| PrintCollector | PrintService Operational log |
| ConfigManager | config.json + auto Low on ≤3 GB RAM |

### Acceptance criteria
- [ ] Low profile: RAM ≤ 15 MB, CPU ≤ 0.3% idle on 2 GB VM
- [ ] File create on removable → event with correlation
- [ ] Network snapshot includes PID + remote address
- [ ] Profile switch via config without recompile

---

## Phase 4 — Tamper Base & Upload

**Goal:** ACL hardening, watchdog, tamper detection, server upload client.

| Module | Notes |
|--------|-------|
| AclGuard | ProgramData ACL, periodic self-check |
| Watchdog | Restart service on failure |
| TamperCollector | Security events + local deny log |
| UploadClient | TLS 1.3 batch upload to ingest API |

### Acceptance criteria
- [ ] Standard user cannot delete `%ProgramData%\UserAudit\`
- [ ] Tamper attempt → `tamper.attempt_denied` event
- [ ] Watchdog restarts service within 60 sec
- [ ] Encrypted blob uploads to server (or local mock in dev)

---

## Phase 5 — Minifilter & IT USB

**Goal:** Kernel protection + cryptographic admin authorization.

| Module | Notes |
|--------|-------|
| UserAudit.sys | Minifilter: deny delete/rename on bin + logs |
| IoctlGuard | Uninstall/stop requires Ed25519 token |
| UserAuditAdmin | IT tool: challenge-response, uninstall |
| UserAuditKeygen | Org key generation ceremony |
| Lockdown mode | Append-only on tamper detect |

### Acceptance criteria
- [ ] Local admin cannot delete logs in running OS (driver loaded)
- [ ] `sc stop` denied without valid IT USB signature
- [ ] Uninstall ceremony logged to server before removal
- [ ] Dev: test-signing instructions documented

---

## Phase 6 — Server Stack

**Goal:** Ingest, key escrow, alerts, web portal.

| Service | Responsibility |
|---------|----------------|
| UserAudit.Ingest | mTLS log upload, storage |
| UserAudit.Escrow | DEK wrap/unwrap, key rotation |
| UserAudit.Alerts | Rule engine, notifications |
| UserAudit.Portal | Host list, timeline, admin UI |

### Acceptance criteria
- [ ] Docker compose brings up all services
- [ ] Agent upload → visible in portal within 5 min
- [ ] Escrow: DEK recoverable by admin role
- [ ] Alert fires on tamper event from agent

---

## Phase 7 — Admin Suite (C#)

**Goal:** WPF dashboard, reports, decrypt, forensic import.

| Module | Notes |
|--------|-------|
| LogImporter | Encrypted JSONL → domain models |
| Dashboard | Timeline, hosts, alerts (WPF) |
| Reports | Excel/PDF (reuse UsbForensicAudit patterns) |
| DecryptTool | Escrow + local unwrap |

### Acceptance criteria
- [ ] Dashboard shows all 15 pilot hosts
- [ ] Daily Activity report: login, top apps, idle time
- [ ] USB report with correlation IDs
- [ ] Forensic pack export (ZIP)

---

## Phase 8 — L3 Forensic

**Goal:** Deep collection — browser, Prefetch, registry, evidence pack.

| Module | Notes |
|--------|-------|
| DeepCollector | Scheduled + trigger (USB) |
| BrowserParser | Chrome, Edge, Firefox SQLite |
| ArtifactParser | Prefetch, Jump Lists, UserAssist |
| EvidencePack | ZIP: JSONL + artifacts |

### Acceptance criteria
- [ ] L3 weekly schedule runs without blocking L1 (separate thread, low priority)
- [ ] Browser history in evidence pack
- [ ] Integration hooks for UsbForensicAudit modules

---

## Phase 9 — Installer & Deploy

**Goal:** MSI silent install, GPO templates, WDAC policy.

| Deliverable | Notes |
|-------------|-------|
| UserAuditSetup.msi | Silent `/quiet`, ACL, service register |
| deploy.ps1 | Machine name, profile, server URL |
| GPO templates | Service protection, BitLocker reminder |
| WDAC policy | Allow signed UserAudit binaries |

### Acceptance criteria
- [ ] Silent install on clean Win10/11 x64
- [ ] Service auto-start after reboot
- [ ] deploy.ps1 completes in < 5 min per machine
- [ ] GPO documented for AD environments

---

## Phase 10 — Release Candidate

**Goal:** Hardening, QA, documentation, signing checklist.

| Deliverable | Notes |
|-------------|-------|
| QA matrix | 2 GB / 4 GB / 8 GB VMs |
| Performance report | RAM, CPU, disk/day |
| Security review | Tamper + crypto |
| Docs | InstallGuide, AdminGuide, SecurityModel (RU) |
| Signing checklist | EV cert, driver HLK |

### Acceptance criteria (Commercial v1.0 RC)
- [ ] All Phase 1–9 criteria met
- [ ] 24h soak test on 2 GB VM: no crash, RAM ≤ 15 MB
- [ ] Pilot-ready deploy package in `dist/`
- [ ] Release notes published

---

## Sprint workflow

Each phase may split into 2–4 sprints. After each sprint:

1. Build + tests pass
2. ROADMAP checkboxes updated
3. Brief summary: done / next

**Current phase:** Phase 1 (Sprint 2 done) → **Next: Phase 1 Sprint 3** (UsbCollector)
