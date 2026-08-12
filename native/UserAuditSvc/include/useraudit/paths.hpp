#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace useraudit {

// Default: %ProgramData%\UserAudit\logs
// Override for dev/tests: set env USERAUDIT_LOG_DIR
std::filesystem::path resolve_log_directory();

std::string get_hostname_utf8();

}  // namespace useraudit
