#pragma once

#include "useraudit/audit_event.hpp"
#include "useraudit/event_sink.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace useraudit {

class JsonlWriter final : public EventSink {
public:
    explicit JsonlWriter(std::filesystem::path log_directory, std::string hostname);

    JsonlWriter(const JsonlWriter&) = delete;
    JsonlWriter& operator=(const JsonlWriter&) = delete;

    bool write(const AuditEvent& event) override;
    bool write_raw_json_line(const std::string& json_line);
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
