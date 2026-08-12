#pragma once

#include "useraudit/config_manager.hpp"
#include "useraudit/sqlite_loader.hpp"

#include <filesystem>
#include <string>

namespace useraudit {

struct BrowserCollectionResult {
    int history_rows = 0;
    int download_rows = 0;
    std::filesystem::path output_jsonl;
};

// Extract Chrome/Edge/Firefox history + downloads into staging/browser/*.jsonl
BrowserCollectionResult collect_browser_artifacts(const ForensicSettings& settings,
                                                  const std::filesystem::path& staging_dir,
                                                  SqliteLoader& sqlite, std::string& error);

std::string chrome_timestamp_to_iso8601(long long webkit_microseconds);

std::string unix_microseconds_to_iso8601(long long unix_microseconds);

}  // namespace useraudit
