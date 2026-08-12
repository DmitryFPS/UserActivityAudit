#include "useraudit/file_collector.hpp"

#include "useraudit/time_utils.hpp"

#include <windows.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace useraudit {

namespace {

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::wstring ensure_trailing_slash(std::wstring path) {
    if (!path.empty() && path.back() != L'\\') {
        path.push_back(L'\\');
    }
    return path;
}

}  // namespace

FileCollector::FileCollector(EventSink& writer, std::string hostname, const ConfigManager& config,
                             CorrelationTracker& correlation)
    : writer_(writer),
      hostname_(std::move(hostname)),
      config_(config),
      correlation_(correlation) {}

FileCollector::~FileCollector() {
    stop();
}

void FileCollector::emit_file_create(const std::wstring& path, const std::string& correlation,
                                   bool removable) {
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) {
        return;
    }
    if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return;
    }

    ULARGE_INTEGER size{};
    size.LowPart = info.nFileSizeLow;
    size.HighPart = info.nFileSizeHigh;

    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 2;
    event.cat = "file";
    event.act = "create";
    event.sev = "info";
    event.host = hostname_;
    event.src = "filesystem";
    if (!correlation.empty()) {
        event.corr = correlation;
    }
    event.data["path"] = wide_to_utf8(path);
    event.data["size"] = std::to_string(size.QuadPart);
    event.data["removable"] = removable ? "true" : "false";
    if (path.size() >= 2 && path[1] == L':') {
        event.data["drive"] = wide_to_utf8(path.substr(0, 2));
    }

    writer_.write(event);
}

void FileCollector::scan_root(const std::wstring& root, const std::string& correlation,
                              bool removable) {
    const std::wstring base = ensure_trailing_slash(root);
    const std::wstring pattern = base + L"*";

    WIN32_FIND_DATAW entry{};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    std::vector<std::wstring> pending_dirs;
    do {
        if (entry.cFileName[0] == L'.' &&
            (entry.cFileName[1] == L'\0' ||
             (entry.cFileName[1] == L'.' && entry.cFileName[2] == L'\0'))) {
            continue;
        }

        const std::wstring full_path = base + entry.cFileName;
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            pending_dirs.push_back(full_path);
            continue;
        }

        const std::string key = wide_to_utf8(full_path);
        bool is_new = false;
        {
            std::lock_guard lock(state_mutex_);
            is_new = known_files_.insert(key).second;
        }
        if (is_new && baseline_ready_) {
            emit_file_create(full_path, correlation, removable);
        }
    } while (FindNextFileW(handle, &entry));
    FindClose(handle);

    for (const auto& dir : pending_dirs) {
        const std::wstring nested = ensure_trailing_slash(dir);
        const std::wstring nested_pattern = nested + L"*";
        WIN32_FIND_DATAW nested_entry{};
        HANDLE nested_handle = FindFirstFileW(nested_pattern.c_str(), &nested_entry);
        if (nested_handle == INVALID_HANDLE_VALUE) {
            continue;
        }

        do {
            if (nested_entry.cFileName[0] == L'.') {
                continue;
            }
            if ((nested_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                continue;
            }

            const std::wstring file_path = nested + nested_entry.cFileName;
            const std::string key = wide_to_utf8(file_path);
            bool is_new = false;
            {
                std::lock_guard lock(state_mutex_);
                is_new = known_files_.insert(key).second;
            }
            if (is_new && baseline_ready_) {
                emit_file_create(file_path, correlation, removable);
            }
        } while (FindNextFileW(nested_handle, &nested_entry));
        FindClose(nested_handle);
    }
}

void FileCollector::poll_thread_main() {
    while (running_.load()) {
        if (stop_flag_ != nullptr && *stop_flag_) {
            break;
        }

        const DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if ((drives & (1U << i)) == 0) {
                continue;
            }

            const wchar_t letter = static_cast<wchar_t>(L'A' + i);
            const std::wstring root = std::wstring(1, letter) + L":\\";
            if (GetDriveTypeW(root.c_str()) != DRIVE_REMOVABLE) {
                continue;
            }

            const std::string drive = wide_to_utf8(root.substr(0, 2));
            const std::string corr = correlation_.correlation_for_drive(drive);
            scan_root(root, corr, true);
        }

        for (const auto& path : config_.config().critical_paths) {
            scan_root(path, {}, false);
        }
        for (const auto& path : config_.config().sensitive_paths) {
            scan_root(path, {}, false);
        }

        {
            std::lock_guard lock(state_mutex_);
            baseline_ready_ = true;
        }

        const int poll_sec = config_.file_poll_sec();
        for (int tick = 0; tick < poll_sec * 10; ++tick) {
            if (!running_.load() || (stop_flag_ != nullptr && *stop_flag_)) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool FileCollector::start() {
    if (running_.load()) {
        return true;
    }

    running_.store(true);
    poll_thread_ = std::thread([this]() { poll_thread_main(); });
    return true;
}

void FileCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
}

}  // namespace useraudit
