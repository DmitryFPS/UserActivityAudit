#pragma once

#include "useraudit/audit_event.hpp"
#include "useraudit/event_sink.hpp"
#include "useraudit/hash_chain.hpp"
#include "useraudit/key_manager.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace useraudit {

class EncryptedLogWriter final : public EventSink {
public:
    static constexpr std::uint64_t kDefaultMaxFileBytes = 50ULL * 1024ULL * 1024ULL;

    EncryptedLogWriter(std::filesystem::path log_directory, std::string hostname,
                       std::uint64_t max_file_bytes = kDefaultMaxFileBytes);

    EncryptedLogWriter(const EncryptedLogWriter&) = delete;
    EncryptedLogWriter& operator=(const EncryptedLogWriter&) = delete;

    bool initialize();
    bool write(const AuditEvent& event) override;
    bool write_raw_json_line(const std::string& json_line);

    [[nodiscard]] const std::filesystem::path& log_directory() const { return log_directory_; }
    [[nodiscard]] const KeyManager& keys() const { return keys_; }

private:
    bool ensure_open_for_today();
    bool write_encrypted_line(const std::string& sealed_json);
    bool rotate_if_needed();
    std::string current_date_utc() const;
    std::filesystem::path current_file_path() const;

    std::filesystem::path log_directory_;
    std::string hostname_;
    std::uint64_t max_file_bytes_;
    KeyManager keys_;
    HashChain chain_;
    mutable std::mutex mutex_;
    std::ofstream stream_;
    std::string open_date_;
    std::uint32_t part_index_ = 0;
    bool ready_ = false;
};

}  // namespace useraudit
