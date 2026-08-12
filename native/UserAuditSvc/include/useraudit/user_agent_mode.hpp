#pragma once

namespace useraudit {

// Interactive user-session mode (foreground tracking). Invoked internally with:
//   UserAudit.exe --user-agent --session-id <id>
int run_user_agent_mode(int argc, wchar_t** argv);

}  // namespace useraudit
