#include "useraudit/encrypted_log_writer.hpp"

#include "useraudit/event_serializer.hpp"
#include "useraudit/log_crypto.hpp"
#include "useraudit/paths.hpp"

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

EncryptedLogWriter::EncryptedLogWriter(std::filesystem::path log_directory, std::string hostname,
                                       std::uint64_t max_file_bytes)
    : log_directory_(std::move(log_directory)),
      hostname_(std::move(hostname)),
      max_file_bytes_(max_file_bytes) {}

bool EncryptedLogWriter::initialize() {
    if (ready_) {
        return true;
    }

    if (!keys_.initialize(resolve_keys_directory())) {
        return false;
    }

    if (!chain_.initialize(resolve_chain_state_path(), keys_.hmac_key(), KeyManager::kHmacKeySize)) {
        return false;
    }

    ready_ = true;
    return true;
}

std::string EncryptedLogWriter::current_date_utc() const {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    return format_utc_date(st);
}

std::filesystem::path EncryptedLogWriter::current_file_path() const {
    std::string name = open_date_;
    if (part_index_ > 0) {
        name += '.' + std::to_string(part_index_);
    }
    name += kEncryptedLogExtension;
    return log_directory_ / name;
}

bool EncryptedLogWriter::rotate_if_needed() {
    if (!stream_.is_open()) {
        return true;
    }

    const auto path = current_file_path();
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size < max_file_bytes_) {
        return true;
    }

    stream_.close();
    ++part_index_;
    stream_.open(current_file_path(), std::ios::out | std::ios::app | std::ios::binary);
    return stream_.is_open();
}

bool EncryptedLogWriter::ensure_open_for_today() {
    const std::string today = current_date_utc();
    if (stream_.is_open() && open_date_ == today) {
        return rotate_if_needed();
    }

    if (stream_.is_open()) {
        stream_.close();
    }

    std::error_code ec;
    std::filesystem::create_directories(log_directory_, ec);
    if (ec) {
        return false;
    }

    open_date_ = today;
    part_index_ = 0;
    stream_.open(current_file_path(), std::ios::out | std::ios::app | std::ios::binary);
    if (!stream_.is_open()) {
        return false;
    }

    return true;
}

bool EncryptedLogWriter::write_encrypted_line(const std::string& sealed_json) {
    if (sealed_json.size() > kMaxPlainLogLineBytes) {
        return false;
    }

    std::string encrypted_line;
    if (!encrypt_log_line(keys_.dek(), KeyManager::kDekSize, sealed_json, encrypted_line)) {
        return false;
    }

    if (!ensure_open_for_today()) {
        return false;
    }

    if (!rotate_if_needed()) {
        return false;
    }

    stream_ << encrypted_line << '\n';
    stream_.flush();
    return stream_.good();
}

bool EncryptedLogWriter::write(const AuditEvent& event) {
    AuditEvent enriched = event;
    if (enriched.host.empty()) {
        enriched.host = hostname_;
    }

    const std::string json = serialize_event_json(enriched);

    std::lock_guard lock(mutex_);
    if (!ready_ && !initialize()) {
        return false;
    }

    const std::string sealed = chain_.seal_json_line(json);
    if (sealed.empty()) {
        return false;
    }

    return write_encrypted_line(sealed);
}

bool EncryptedLogWriter::write_raw_json_line(const std::string& json_line) {
    if (json_line.empty()) {
        return false;
    }

    std::lock_guard lock(mutex_);
    if (!ready_ && !initialize()) {
        return false;
    }

    const std::string sealed = chain_.seal_json_line(json_line);
    if (sealed.empty()) {
        return false;
    }

    return write_encrypted_line(sealed);
}

}  // namespace useraudit
