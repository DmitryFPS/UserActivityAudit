#include "useraudit/config_manager.hpp"

#include "useraudit/paths.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace useraudit {

namespace {

std::string read_file_utf8(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

bool extract_quoted_string(const std::string& json, const std::string& key, std::string& out) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }

    const auto colon = json.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return false;
    }

    const auto quote_start = json.find('"', colon + 1);
    if (quote_start == std::string::npos) {
        return false;
    }

    std::string value;
    bool escape = false;
    for (size_t i = quote_start + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (escape) {
            value.push_back(ch);
            escape = false;
            continue;
        }
        if (ch == '\\') {
            escape = true;
            continue;
        }
        if (ch == '"') {
            out = value;
            return true;
        }
        value.push_back(ch);
    }
    return false;
}

bool extract_bool(const std::string& json, const std::string& key, bool& out) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }

    const auto colon = json.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return false;
    }

    const auto value_start = json.find_first_not_of(" \t\r\n", colon + 1);
    if (value_start == std::string::npos) {
        return false;
    }

    if (json.compare(value_start, 4, "true") == 0) {
        out = true;
        return true;
    }
    if (json.compare(value_start, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

bool extract_uint64(const std::string& json, const std::string& key, std::uint64_t& out) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }

    const auto colon = json.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return false;
    }

    const auto value_start = json.find_first_of("0123456789", colon + 1);
    if (value_start == std::string::npos) {
        return false;
    }

    out = 0;
    for (size_t i = value_start; i < json.size(); ++i) {
        const char ch = json[i];
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            break;
        }
        out = out * 10 + static_cast<std::uint64_t>(ch - '0');
    }
    return true;
}

bool extract_int(const std::string& json, const std::string& key, int& out) {
    std::uint64_t value = 0;
    if (!extract_uint64(json, key, value)) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool extract_string_array(const std::string& json, const std::string& key,
                          std::vector<std::string>& out) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }

    const auto bracket = json.find('[', key_pos);
    if (bracket == std::string::npos) {
        return false;
    }

    const auto end = json.find(']', bracket);
    if (end == std::string::npos) {
        return false;
    }

    out.clear();
    size_t pos = bracket + 1;
    while (pos < end) {
        const auto quote = json.find('"', pos);
        if (quote == std::string::npos || quote >= end) {
            break;
        }

        std::string value;
        bool escape = false;
        size_t i = quote + 1;
        for (; i < end; ++i) {
            const char ch = json[i];
            if (escape) {
                value.push_back(ch);
                escape = false;
                continue;
            }
            if (ch == '\\') {
                escape = true;
                continue;
            }
            if (ch == '"') {
                break;
            }
            value.push_back(ch);
        }
        if (!value.empty()) {
            out.push_back(value);
        }
        pos = i + 1;
    }
    return true;
}

std::string extract_object_body(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return {};
    }

    const auto brace = json.find('{', key_pos);
    if (brace == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (size_t i = brace; i < json.size(); ++i) {
        if (json[i] == '{') {
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(brace, i - brace + 1);
            }
        }
    }
    return {};
}

unsigned long long total_ram_mb() {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) {
        return 4096;
    }
    return static_cast<unsigned long long>(status.ullTotalPhys / (1024ULL * 1024ULL));
}

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

}  // namespace

AuditProfile detect_profile_from_ram_mb(unsigned long long ram_mb) {
    return ram_mb <= 3072 ? AuditProfile::Low : AuditProfile::Standard;
}

std::wstring expand_config_path(const std::wstring& path) {
    if (path.empty()) {
        return {};
    }

    wchar_t buffer[MAX_PATH * 4]{};
    const DWORD size = ExpandEnvironmentStringsW(path.c_str(), buffer,
                                                 static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
    if (size == 0 || size >= sizeof(buffer) / sizeof(buffer[0])) {
        return path;
    }
    return buffer;
}

std::vector<std::wstring> expand_user_profile_paths(const std::wstring& template_path) {
    if (template_path.find(L"%USERPROFILE%") == std::wstring::npos) {
        return {expand_config_path(template_path)};
    }

    std::vector<std::wstring> paths;
    const std::wstring users_root = L"C:\\Users\\*";
    WIN32_FIND_DATAW entry{};
    HANDLE handle = FindFirstFileW(users_root.c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        return {expand_config_path(template_path)};
    }

    do {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        if (entry.cFileName[0] == L'.') {
            continue;
        }
        if (_wcsicmp(entry.cFileName, L"Public") == 0 || _wcsicmp(entry.cFileName, L"Default") == 0 ||
            _wcsicmp(entry.cFileName, L"Default User") == 0 ||
            _wcsicmp(entry.cFileName, L"All Users") == 0) {
            continue;
        }

        std::wstring path = template_path;
        const std::wstring profile = std::wstring(L"C:\\Users\\") + entry.cFileName;
        size_t pos = 0;
        while ((pos = path.find(L"%USERPROFILE%", pos)) != std::wstring::npos) {
            path.replace(pos, 13, profile);
            pos += profile.size();
        }
        paths.push_back(expand_config_path(path));
    } while (FindNextFileW(handle, &entry));

    FindClose(handle);
    return paths.empty() ? std::vector<std::wstring>{expand_config_path(template_path)} : paths;
}

bool parse_agent_config_json(const std::string& json, AgentConfig& out, std::string& error) {
    AgentConfig config{};

    std::string profile_name = "auto";
    extract_quoted_string(json, "profile", profile_name);
    config.profile_name = profile_name;

    const std::string collectors_json = extract_object_body(json, "collectors");
    if (!collectors_json.empty()) {
        extract_bool(collectors_json, "session", config.collectors.session);
        extract_bool(collectors_json, "process", config.collectors.process);
        extract_bool(collectors_json, "foreground", config.collectors.foreground);
        extract_bool(collectors_json, "usb", config.collectors.usb);
        extract_bool(collectors_json, "file", config.collectors.file);
        extract_bool(collectors_json, "network", config.collectors.network);
        extract_bool(collectors_json, "clipboard", config.collectors.clipboard);
        extract_bool(collectors_json, "print", config.collectors.print);
    }

    const std::string paths_json = extract_object_body(json, "paths");
    if (!paths_json.empty()) {
        std::vector<std::string> critical;
        std::vector<std::string> sensitive;
        extract_string_array(paths_json, "critical", critical);
        extract_string_array(paths_json, "sensitive", sensitive);

        for (const auto& item : critical) {
            const auto expanded = expand_user_profile_paths(utf8_to_wide(item));
            config.critical_paths.insert(config.critical_paths.end(), expanded.begin(),
                                         expanded.end());
        }
        for (const auto& item : sensitive) {
            const auto expanded = expand_user_profile_paths(utf8_to_wide(item));
            config.sensitive_paths.insert(config.sensitive_paths.end(), expanded.begin(),
                                          expanded.end());
        }
    }

    const std::string storage_json = extract_object_body(json, "storage");
    if (!storage_json.empty()) {
        extract_uint64(storage_json, "max_log_mb_per_day", config.max_log_mb_per_day);
    }

    const std::string server_json = extract_object_body(json, "server");
    if (!server_json.empty()) {
        extract_quoted_string(server_json, "ingest_url", config.ingest_url);
        extract_quoted_string(server_json, "mtls_cert_thumbprint", config.mtls_cert_thumbprint);
        extract_int(server_json, "upload_interval_minutes", config.upload_interval_minutes);
    }

    if (profile_name == "auto") {
        config.profile = detect_profile_from_ram_mb(total_ram_mb());
    } else if (profile_name == "low") {
        config.profile = AuditProfile::Low;
    } else if (profile_name == "full") {
        config.profile = AuditProfile::Full;
    } else {
        config.profile = AuditProfile::Standard;
    }

    switch (config.profile) {
        case AuditProfile::Low:
            config.window_poll_sec = 5;
            config.network_poll_sec = 60;
            config.file_poll_sec = 30;
            config.clipboard_poll_sec = 15;
            if (config.max_log_mb_per_day > 3) {
                config.max_log_mb_per_day = 3;
            }
            break;
        case AuditProfile::Full:
            config.window_poll_sec = 2;
            config.network_poll_sec = 15;
            config.file_poll_sec = 10;
            config.clipboard_poll_sec = 5;
            if (config.max_log_mb_per_day < 50) {
                config.max_log_mb_per_day = 50;
            }
            break;
        case AuditProfile::Standard:
        default:
            config.window_poll_sec = 3;
            config.network_poll_sec = 30;
            config.file_poll_sec = 15;
            config.clipboard_poll_sec = 10;
            break;
    }

    out = config;
    error.clear();
    return true;
}

void ConfigManager::apply_profile(AuditProfile profile) {
    config_.profile = profile;
    switch (profile) {
        case AuditProfile::Low:
            config_.window_poll_sec = 5;
            config_.network_poll_sec = 60;
            config_.file_poll_sec = 30;
            config_.clipboard_poll_sec = 15;
            break;
        case AuditProfile::Full:
            config_.window_poll_sec = 2;
            config_.network_poll_sec = 15;
            config_.file_poll_sec = 10;
            config_.clipboard_poll_sec = 5;
            break;
        case AuditProfile::Standard:
        default:
            config_.window_poll_sec = 3;
            config_.network_poll_sec = 30;
            config_.file_poll_sec = 15;
            config_.clipboard_poll_sec = 10;
            break;
    }
}

AuditProfile ConfigManager::resolve_profile_name(const std::string& name) const {
    if (name == "auto") {
        return detect_profile_from_ram_mb(total_ram_mb());
    }
    if (name == "low") {
        return AuditProfile::Low;
    }
    if (name == "full") {
        return AuditProfile::Full;
    }
    return AuditProfile::Standard;
}

bool ConfigManager::write_default_config(const std::filesystem::path& path) const {
    static constexpr char kDefaultConfig[] = R"({
  "version": 1,
  "profile": "auto",
  "server": {
    "ingest_url": "https://127.0.0.1:8443/mock",
    "mtls_cert_thumbprint": "",
    "upload_interval_minutes": 120
  },
  "collectors": {
    "session": true,
    "process": true,
    "foreground": true,
    "usb": true,
    "file": true,
    "network": true,
    "clipboard": false,
    "print": true
  },
  "paths": {
    "critical": [
      "%USERPROFILE%\\Documents",
      "%USERPROFILE%\\Desktop",
      "%USERPROFILE%\\Downloads"
    ],
    "sensitive": []
  },
  "storage": {
    "max_log_mb_per_day": 10,
    "retention_days": 90
  }
})";

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output.write(kDefaultConfig, static_cast<std::streamsize>(sizeof(kDefaultConfig) - 1));
    return output.good();
}

bool ConfigManager::parse_file(const std::filesystem::path& path) {
    const std::string json = read_file_utf8(path);
    if (json.empty()) {
        return false;
    }

    std::string error;
    AgentConfig parsed{};
    if (!parse_agent_config_json(json, parsed, error)) {
        return false;
    }

    config_ = parsed;
    config_.profile = resolve_profile_name(config_.profile_name);
    apply_profile(config_.profile);
    return true;
}

bool ConfigManager::load() {
    config_path_ = resolve_config_path();
    if (!std::filesystem::exists(config_path_)) {
        write_default_config(config_path_);
    }

    std::error_code ec;
    last_write_time_ = std::filesystem::last_write_time(config_path_, ec);
    return parse_file(config_path_);
}

bool ConfigManager::reload_if_modified() {
    if (config_path_.empty()) {
        return false;
    }

    std::error_code ec;
    const auto current = std::filesystem::last_write_time(config_path_, ec);
    if (ec || current == last_write_time_) {
        return false;
    }

    last_write_time_ = current;
    return parse_file(config_path_);
}

bool ConfigManager::collector_enabled(const char* name) const {
    if (name == nullptr) {
        return false;
    }
    const std::string key = name;
    if (key == "session") return config_.collectors.session;
    if (key == "process") return config_.collectors.process;
    if (key == "foreground") return config_.collectors.foreground;
    if (key == "usb") return config_.collectors.usb;
    if (key == "file") return config_.collectors.file;
    if (key == "network") return config_.collectors.network;
    if (key == "clipboard") return config_.collectors.clipboard;
    if (key == "print") return config_.collectors.print;
    return false;
}

}  // namespace useraudit
