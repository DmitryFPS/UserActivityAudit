#include "useraudit/jsonl_writer.hpp"

#include "useraudit/event_serializer.hpp"

#include <windows.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace useraudit {

namespace {

std::string format_utc_date(const SYSTEMTIME& st) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << st.wYear << '-' << std::setw(2) << st.wMonth
        << '-' << std::setw(2) << st.wDay;
    return oss.str();
}

}  // namespace

JsonlWriter::JsonlWriter(std::filesystem::path log_directory, std::string hostname)
    : log_directory_(std::move(log_directory)), hostname_(std::move(hostname)) {}

bool JsonlWriter::ensure_open_for_today() {
    const std::string today = current_date_utc();
    if (stream_.is_open() && open_date_ == today) {
        return true;
    }

    if (stream_.is_open()) {
        stream_.close();
    }

    std::error_code ec;
    std::filesystem::create_directories(log_directory_, ec);
    if (ec) {
        return false;
    }

    const auto file_path = log_directory_ / (today + ".jsonl");
    stream_.open(file_path, std::ios::out | std::ios::app | std::ios::binary);
    if (!stream_.is_open()) {
        return false;
    }

    open_date_ = today;
    return true;
}

std::string JsonlWriter::current_date_utc() const {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    return format_utc_date(st);
}

bool JsonlWriter::write(const AuditEvent& event) {
    AuditEvent enriched = event;
    if (enriched.host.empty()) {
        enriched.host = hostname_;
    }

    const std::string line = serialize_event_json(enriched);

    std::lock_guard lock(mutex_);
    if (!ensure_open_for_today()) {
        return false;
    }

    stream_ << line << '\n';
    stream_.flush();
    return stream_.good();
}

bool JsonlWriter::write_raw_json_line(const std::string& json_line) {
    if (json_line.empty()) {
        return false;
    }

    std::lock_guard lock(mutex_);
    if (!ensure_open_for_today()) {
        return false;
    }

    stream_ << json_line << '\n';
    stream_.flush();
    return stream_.good();
}

}  // namespace useraudit
