#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace useraudit {

// Log directory: %ProgramData%\UserAudit\logs
std::filesystem::path resolve_log_directory();

std::string get_hostname_utf8();

}  // namespace useraudit
