#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace useraudit {

// Log directory: %ProgramData%\UserAudit\logs
std::filesystem::path resolve_log_directory();

// Key material: %ProgramData%\UserAudit\keys
std::filesystem::path resolve_keys_directory();

// Agent config: %ProgramData%\UserAudit\config.json
std::filesystem::path resolve_config_path();

// Hash chain state: %ProgramData%\UserAudit\keys\chain.state
std::filesystem::path resolve_chain_state_path();

// Data root: %ProgramData%\UserAudit
std::filesystem::path resolve_data_root();

// Mock upload outbox: %ProgramData%\UserAudit\outbox
std::filesystem::path resolve_outbox_directory();

// Upload cursor: %ProgramData%\UserAudit\keys\upload.state
std::filesystem::path resolve_upload_state_path();

std::string get_hostname_utf8();

}  // namespace useraudit
