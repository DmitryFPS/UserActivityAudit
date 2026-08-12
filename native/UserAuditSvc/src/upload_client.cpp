#include "useraudit/upload_client.hpp"

#include "useraudit/paths.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace useraudit {

namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool read_file_bytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0) {
        return false;
    }
    out.resize(static_cast<size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(out.data()), size);
    return input.good();
}

URL_COMPONENTS parse_url(std::wstring url) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    WinHttpCrackUrl(url.c_str(), 0, 0, &parts);
    return parts;
}

std::wstring component_to_string(const std::wstring& url, const WCHAR* ptr, DWORD length) {
    if (ptr == nullptr || length == 0) {
        return {};
    }
    return url.substr(static_cast<size_t>(ptr - url.c_str()), length);
}

}  // namespace

UploadSettings parse_upload_settings(const AgentConfig& config) {
    UploadSettings settings{};
    settings.ingest_url = config.ingest_url;
    settings.mtls_cert_thumbprint = config.mtls_cert_thumbprint;
    settings.upload_interval_minutes = config.upload_interval_minutes;

    if (settings.ingest_url.empty()) {
        settings.mode = UploadMode::Disabled;
        return settings;
    }

    const std::string lower = to_lower_ascii(settings.ingest_url);
    if (lower.find("mock") != std::string::npos) {
        settings.mode = UploadMode::MockOutbox;
        return settings;
    }

    settings.mode = UploadMode::Http;
    return settings;
}

UploadStateStore::UploadStateStore(std::filesystem::path state_path)
    : state_path_(std::move(state_path)) {
    load();
}

void UploadStateStore::load() {
    uploaded_.clear();
    std::ifstream input(state_path_);
    if (!input.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            uploaded_.push_back(line);
        }
    }
}

void UploadStateStore::save() const {
    std::error_code ec;
    std::filesystem::create_directories(state_path_.parent_path(), ec);
    std::ofstream output(state_path_, std::ios::trunc);
    if (!output.is_open()) {
        return;
    }
    for (const auto& entry : uploaded_) {
        output << entry << '\n';
    }
}

bool UploadStateStore::is_uploaded(const std::filesystem::path& log_file) const {
    const std::string key = log_file.generic_string();
    return std::find(uploaded_.begin(), uploaded_.end(), key) != uploaded_.end();
}

void UploadStateStore::mark_uploaded(const std::filesystem::path& log_file) {
    const std::string key = log_file.generic_string();
    if (is_uploaded(log_file)) {
        return;
    }
    uploaded_.push_back(key);
    save();
}

UploadClient::UploadClient(std::filesystem::path log_directory, UploadSettings settings)
    : log_directory_(std::move(log_directory)),
      settings_(std::move(settings)),
      state_(resolve_upload_state_path()) {}

UploadClient::~UploadClient() {
    stop();
}

bool UploadClient::upload_mock(const std::filesystem::path& path,
                               const std::vector<std::uint8_t>& body) {
    const auto outbox = resolve_outbox_directory();
    std::error_code ec;
    std::filesystem::create_directories(outbox, ec);
    const auto destination = outbox / path.filename();
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output.write(reinterpret_cast<const char*>(body.data()),
                 static_cast<std::streamsize>(body.size()));
    return output.good();
}

bool UploadClient::upload_http(const std::filesystem::path& path,
                               const std::vector<std::uint8_t>& body) {
    const std::wstring wide_url(settings_.ingest_url.begin(), settings_.ingest_url.end());
    URL_COMPONENTS parts = parse_url(wide_url);
    const std::wstring host = component_to_string(wide_url, parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring url_path =
        component_to_string(wide_url, parts.lpszUrlPath, parts.dwUrlPathLength);
    const std::wstring request_path =
        url_path.empty() ? L"/api/v1/ingest/logs" : url_path + L"/api/v1/ingest/logs";

    HINTERNET session =
        WinHttpOpen(L"UserAuditAgent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        return false;
    }

    const INTERNET_PORT port =
        parts.nPort != 0 ? parts.nPort
                         : (parts.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
                                                                   : INTERNET_DEFAULT_HTTP_PORT);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (connect == nullptr) {
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD flags =
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", request_path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           flags);
    if (request == nullptr) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const std::wstring host_header = L"Host: " + host;
    WinHttpAddRequestHeaders(request, host_header.c_str(), static_cast<DWORD>(-1),
                             WINHTTP_ADDREQ_FLAG_ADD);

    const std::wstring file_header =
        L"X-Log-File: " + path.filename().wstring();
    WinHttpAddRequestHeaders(request, file_header.c_str(), static_cast<DWORD>(-1),
                             WINHTTP_ADDREQ_FLAG_ADD);

    const BOOL sent = WinHttpSendRequest(
        request, L"Content-Type: application/octet-stream\r\n", static_cast<DWORD>(-1),
        const_cast<void*>(static_cast<const void*>(body.data())), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                        WINHTTP_NO_HEADER_INDEX);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return status_code >= 200 && status_code < 300;
}

bool UploadClient::upload_file(const std::filesystem::path& path) {
    std::vector<std::uint8_t> body;
    if (!read_file_bytes(path, body)) {
        return false;
    }

    bool ok = false;
    switch (settings_.mode) {
        case UploadMode::MockOutbox:
            ok = upload_mock(path, body);
            break;
        case UploadMode::Http:
            ok = upload_http(path, body);
            break;
        case UploadMode::Disabled:
        default:
            return false;
    }

    if (ok) {
        state_.mark_uploaded(path);
    }
    return ok;
}

std::size_t UploadClient::run_once() {
    if (settings_.mode == UploadMode::Disabled) {
        return 0;
    }

    std::error_code ec;
    if (!std::filesystem::exists(log_directory_, ec)) {
        return 0;
    }

    std::size_t uploaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(log_directory_, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != L".enc") {
            continue;
        }
        if (state_.is_uploaded(entry.path())) {
            continue;
        }
        if (upload_file(entry.path())) {
            ++uploaded;
        }
    }
    return uploaded;
}

void UploadClient::upload_thread_main() {
    const int interval_minutes =
        settings_.upload_interval_minutes < 1 ? 1 : settings_.upload_interval_minutes;
    while (running_.load() && (stop_flag_ == nullptr || !*stop_flag_)) {
        run_once();
        for (int minute = 0; minute < interval_minutes; ++minute) {
            for (int second = 0; second < 60; ++second) {
                if (!running_.load() || (stop_flag_ != nullptr && *stop_flag_)) {
                    running_.store(false);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    running_.store(false);
}

bool UploadClient::start() {
    if (settings_.mode == UploadMode::Disabled) {
        return true;
    }
    if (running_.load()) {
        return true;
    }
    running_.store(true);
    upload_thread_ = std::thread([this]() { upload_thread_main(); });
    return true;
}

void UploadClient::stop() {
    running_.store(false);
    if (upload_thread_.joinable()) {
        upload_thread_.join();
    }
}

}  // namespace useraudit
