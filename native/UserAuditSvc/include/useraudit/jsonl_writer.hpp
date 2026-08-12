#pragma once

#include "useraudit/audit_event.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace useraudit {

class JsonlWriter {
public:
    explicit JsonlWriter(std::filesystem::path log_directory, std::string hostname);

    JsonlWriter(const JsonlWriter&) = delete;
    JsonlWriter& operator=(const JsonlWriter&) = delete;

    bool write(const AuditEvent& event);
    [[nodiscard]] const std::filesystem::path& log_directory() const { return log_directory_; }

private:
    bool ensure_open_for_today();
    std::string current_date_utc() const;

    std::filesystem::path log_directory_;
    std::string hostname_;
    mutable std::mutex mutex_;
    std::ofstream stream_;
    std::string open_date_;
};

}  // namespace useraudit
