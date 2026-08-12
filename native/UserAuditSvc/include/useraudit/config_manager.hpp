#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace useraudit {

enum class AuditProfile { Low, Standard, Full };

struct CollectorToggles {
    bool session = true;
    bool process = true;
    bool foreground = true;
    bool usb = true;
    bool file = true;
    bool network = true;
    bool clipboard = false;
    bool print = true;
};

struct AgentConfig {
    int version = 1;
    std::string profile_name = "auto";
    AuditProfile profile = AuditProfile::Standard;
    CollectorToggles collectors{};
    std::vector<std::wstring> critical_paths;
    std::vector<std::wstring> sensitive_paths;
    int window_poll_sec = 3;
    int network_poll_sec = 30;
    int file_poll_sec = 15;
    int clipboard_poll_sec = 10;
    std::uint64_t max_log_mb_per_day = 10;
    std::string ingest_url;
    std::string mtls_cert_thumbprint;
    int upload_interval_minutes = 120;
};

class ConfigManager {
public:
    bool load();
    bool reload_if_modified();

    [[nodiscard]] const AgentConfig& config() const { return config_; }
    [[nodiscard]] AuditProfile profile() const { return config_.profile; }
    [[nodiscard]] int window_poll_sec() const { return config_.window_poll_sec; }
    [[nodiscard]] int network_poll_sec() const { return config_.network_poll_sec; }
    [[nodiscard]] int file_poll_sec() const { return config_.file_poll_sec; }
    [[nodiscard]] int clipboard_poll_sec() const { return config_.clipboard_poll_sec; }
    [[nodiscard]] std::uint64_t max_log_mb_per_day() const { return config_.max_log_mb_per_day; }
    [[nodiscard]] bool collector_enabled(const char* name) const;
    [[nodiscard]] const std::string& ingest_url() const { return config_.ingest_url; }
    [[nodiscard]] int upload_interval_minutes() const { return config_.upload_interval_minutes; }
    [[nodiscard]] const AgentConfig& raw_config() const { return config_; }

private:
    bool parse_file(const std::filesystem::path& path);
    bool write_default_config(const std::filesystem::path& path) const;
    void apply_profile(AuditProfile profile);
    AuditProfile resolve_profile_name(const std::string& name) const;

    AgentConfig config_{};
    std::filesystem::path config_path_;
    std::filesystem::file_time_type last_write_time_{};
};

// Для unit-тестов и установщика.
bool parse_agent_config_json(const std::string& json, AgentConfig& out, std::string& error);
std::wstring expand_config_path(const std::wstring& path);
std::vector<std::wstring> expand_user_profile_paths(const std::wstring& template_path);
AuditProfile detect_profile_from_ram_mb(unsigned long long ram_mb);

}  // namespace useraudit
