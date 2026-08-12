#pragma once

#include <string>

namespace useraudit {

// Current UTC time: 2026-08-12T10:15:30.123Z
std::string utc_now_iso8601();

// New random UUID v4 string (lowercase hex with dashes).
std::string generate_event_id();

// Parse XML SystemTime attribute from Security event TimeCreated, fallback to utc_now_iso8601().
std::string iso8601_from_event_xml(const std::wstring& xml);

}  // namespace useraudit
