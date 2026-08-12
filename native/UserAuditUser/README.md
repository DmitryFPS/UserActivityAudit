# UserAuditUser — interactive session agent

Runs in the **logged-on user's session** (not LocalSystem). Collects foreground window events and sends them to `UserAuditSvc` via named pipe.

Launched automatically by `SessionAgentManager` on login. Binary must sit next to `UserAuditSvc.exe`.
