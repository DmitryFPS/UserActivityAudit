#pragma once

#include "useraudit/config_manager.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace useraudit {

enum class UploadMode { Disabled, Http, MockOutbox };

struct UploadSettings {
    std::string ingest_url;
    std::string mtls_cert_thumbprint;
    int upload_interval_minutes = 120;
    UploadMode mode = UploadMode::Disabled;
};

// Parses server section from config.json.
UploadSettings parse_upload_settings(const AgentConfig& config);

// Tracks uploaded log files on disk (%ProgramData%\\UserAudit\\keys\\upload.state).
class UploadStateStore {
public:
    explicit UploadStateStore(std::filesystem::path state_path);

    [[nodiscard]] bool is_uploaded(const std::filesystem::path& log_file) const;
    void mark_uploaded(const std::filesystem::path& log_file);

private:
    void load();
    void save() const;

    std::filesystem::path state_path_;
    std::vector<std::string> uploaded_;
};

// Batch upload of encrypted log blobs (TLS via WinHTTP; mock copies to outbox/).
class UploadClient {
public:
    UploadClient(std::filesystem::path log_directory, UploadSettings settings);
    ~UploadClient();

    UploadClient(const UploadClient&) = delete;
    UploadClient& operator=(const UploadClient&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

    // Runs one upload cycle immediately (for tests / --upload-now).
    std::size_t run_once();

private:
    void upload_thread_main();
    bool upload_file(const std::filesystem::path& path);
    bool upload_http(const std::filesystem::path& path, const std::vector<std::uint8_t>& body);
    bool upload_mock(const std::filesystem::path& path, const std::vector<std::uint8_t>& body);

    std::filesystem::path log_directory_;
    UploadSettings settings_;
    UploadStateStore state_;
    volatile bool* stop_flag_ = nullptr;
    std::thread upload_thread_;
    std::atomic<bool> running_{false};
};

}  // namespace useraudit
