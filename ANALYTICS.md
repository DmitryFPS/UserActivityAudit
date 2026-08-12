# UserActivityAudit — Product Analytics (Commercial v1.0)

Full specification consolidated from product design sessions.  
**Mode:** Commercial production. **Pilot fleet:** 15 laptops (2 GB RAM supported).

---

## 1. Product summary

| Item | Value |
|------|-------|
| Product | User activity audit for Windows 10/11 |
| Client | C++20 Windows Service, ≤15 MB RAM (Low profile) |
| Server | .NET 10 — ingest, escrow, alerts, web portal |
| Admin | .NET 10 — WPF dashboard, Excel/PDF, forensic |
| Security | AES-256-GCM, TPM+USB split key, minifilter, IT USB |
| Deployment | MSI silent, GPO, Docker server |

**UTP:** Forensic-grade audit + ultra-light agent + anti-admin tamper + encrypted logs.

---

## 2. Goals & scenarios

- Corporate monitoring (time, apps, compliance)
- IB / incident investigation (timeline, USB, exfiltration)
- Forensic evidence packs
- Compliance (152-FZ, GDPR, ISO 27001)

**Realistic coverage:** ≥95% of typical user actions with documented gaps.

---

## 3. Threat model

| Actor | Mitigation |
|-------|------------|
| Standard user | ACL + Service SYSTEM + encrypted logs |
| Local admin | Minifilter + IT USB + BitLocker |
| Offline attack | BitLocker |
| Log file leak | Split DEK (TPM ⊕ USB), server escrow |
| Log tampering | HMAC hash chain + server anchor |

---

## 4. Collection tiers

### L1 — Real-time (always)
- Session: login/logout/lock/unlock/idle/RDP (4624, 4634, 4800, 4801)
- Process: ETW Kernel-Process, parent chain, command line, SHA256 (selective)
- Foreground: Application Sessions, poll 3–5 sec
- USB: insert/remove, VID/PID/serial

### L2 — Near-real-time
- Files: tiered paths + removable + SACL sensitive paths
- Network: TCP/UDP, DNS, bytes/PID (30–60 sec)
- Clipboard: hash only (plaintext opt-in)
- Print, privilege events, Wi-Fi SSID

### L3 — Deep / Forensic (scheduled + trigger)
- Browser history/downloads (Chrome, Edge, Firefox)
- Prefetch, Amcache, Jump Lists, UserAssist
- Registry USB/Run/Tasks, cloud sync logs
- Evidence Pack ZIP

### Optional modules (off by default)
- Screenshots on trigger
- Keylog (opt-in, legal gate)
- Screen record (investigation only)

---

## 5. Alerts

| Rule | Severity |
|------|----------|
| USB + copy > 10 MB | High |
| PowerShell -enc / run from TEMP | Critical |
| Mass delete > 50/min | High |
| Tamper / stop service attempt | Critical |
| Denylist process | Critical |
| Large upload anomalous PID | High |

---

## 6. Profiles

| Param | Low (2 GB) | Standard | Full |
|-------|------------|----------|------|
| window_poll_sec | 5 | 3 | 2 |
| network_poll_sec | 60 | 30 | 15 |
| L3 schedule | weekly/trigger | nightly | nightly |
| screenshots | off | trigger | trigger |
| max_log_mb_day | 3 | 10 | 50 |

Auto-detect: RAM ≤ 3072 → Low.

---

## 7. Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
Client: UserAudit.sys + UserAuditSvc + Watchdog
Server: Ingest + Escrow + Alerts + Portal (Docker)
Admin: Dashboard + Reports + Forensic + UserAuditAdmin (IT USB)
```

---

## 8. Event schema (JSONL)

```json
{
  "id": "uuid-v7",
  "seq": 1001,
  "prev_hmac": "...",
  "ts": "2026-08-12T10:00:00.000Z",
  "lvl": 1,
  "cat": "session|process|window|file|network|usb|...",
  "act": "login|start|focus|create|insert|attempt_denied",
  "sev": "info|warning|critical",
  "host": "NB-01",
  "user": "DOMAIN\\user",
  "sid": "S-1-5-21-...",
  "corr": "uuid",
  "data": {}
}
```

On disk: AES-256-GCM encrypted (Phase 2+).

---

## 9. Security stack

### Encryption
- AES-256-GCM streaming (BCrypt)
- DEK = K_machine (TPM-sealed) ⊕ K_usb (32-byte binary on IT USB)
- Server escrow (RSA-OAEP wrap DEK)
- HMAC hash chain per event

### Tamper
- L1: Service LocalSystem, ACL Users=None
- L2: Watchdog, SCM recovery, tamper events
- L3: GPO, AppLocker/WDAC
- L4: Minifilter deny delete/rename
- L5: BitLocker, remote backup

### IT USB
- Ed25519 challenge-response for uninstall/stop
- UserAuditKeygen ceremony at deploy
- Lockdown mode on tamper detect

---

## 10. Performance (2 GB target)

| Metric | Target |
|--------|--------|
| RAM | ≤ 15 MB |
| CPU idle | ≤ 0.3% |
| Disk/day | ≤ 10 MB |
| Service delay after boot | 60–120 sec |

---

## 11. Server (commercial)

| Service | Role |
|---------|------|
| UserAudit.Ingest | mTLS upload, storage |
| UserAudit.Escrow | DEK vault, rotation |
| UserAudit.Alerts | Rule engine, notify |
| UserAudit.Portal | Hosts, timeline, admin |

---

## 12. Admin suite

- **Dashboard (WPF):** live timeline, 15 hosts, alerts
- **Reports:** Daily Activity, USB, Incident Timeline, Compliance
- **Forensic:** Evidence pack, UsbForensicAudit integration
- **UserAuditAdmin:** decrypt, uninstall ceremony

---

## 13. Legal (RU)

- Employee notification + monitoring policy
- 152-FZ if PII in logs
- Keylog/screenshots: separate consent
- Retention default 90 days

---

## 14. External requirements

- EV code signing
- Microsoft driver signing (HLK) for UserAudit.sys
- Server (VPS/on-prem)
- IT USB with org key
- BitLocker on pilot laptops

---

## 15. Integration

| Project | Reuse |
|---------|-------|
| UsbForensicAudit | USB parsers, Excel/PDF, WlanApi |
| AutoConfigSec | auditpol, registry audit GPO prep |

---

## 16. Acceptance criteria (Commercial v1.0 RC)

1. L1+L2+L3 by profile; RAM ≤15 MB on 2 GB
2. Admin cannot delete logs without IT USB (driver)
3. Leaked log file unreadable without escrow
4. Portal shows all pilot hosts; alerts <60 sec
5. WPF + Excel/PDF + Forensic pack
6. MSI silent install + docs
7. 24h soak test pass on 2 GB VM

---

## 17. Roadmap

See [ROADMAP.md](ROADMAP.md) — Phases 0–10.

**Current:** Phase 0 complete → Phase 1 (L1 collectors) next.
