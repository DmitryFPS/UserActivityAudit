#pragma once

#include "useraudit/audit_event.hpp"

#include <string>

namespace useraudit {

// Escape a UTF-8 string for JSON string value (no surrounding quotes).
std::string json_escape(std::string_view input);

// Serialize one audit event to a single JSON line (no trailing newline).
std::string serialize_event_json(const AuditEvent& event);

}  // namespace useraudit
