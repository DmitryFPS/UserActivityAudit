#include "useraudit/foreground_collector.hpp"

#include "useraudit/time_utils.hpp"

#include <windows.h>
#include <psapi.h>

#include <chrono>
#include <string>
#include <thread>

namespace useraudit {

namespace {

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::wstring query_process_path(unsigned long pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return {};
    }

    std::wstring path;
    path.resize(MAX_PATH);
    DWORD size = static_cast<DWORD>(path.size());
    if (QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
        path.resize(size);
    } else {
        path.clear();
    }
    CloseHandle(process);
    return path;
}

std::wstring query_window_title(HWND hwnd) {
    std::wstring title;
    title.resize(512);
    const int length =
        GetWindowTextW(hwnd, title.data(), static_cast<int>(title.size()));
    if (length <= 0) {
        return {};
    }
    title.resize(static_cast<size_t>(length));
    return title;
}

}  // namespace

ForegroundCollector::ForegroundCollector(JsonlWriter& writer, std::string hostname,
                                         int poll_interval_sec)
    : writer_(writer),
      hostname_(std::move(hostname)),
      poll_interval_sec_(poll_interval_sec > 0 ? poll_interval_sec : 5) {}

ForegroundCollector::~ForegroundCollector() {
    stop();
}

bool ForegroundCollector::emit_focus_if_changed() {
    const HWND hwnd = GetForegroundWindow();
    if (hwnd == nullptr) {
        return false;
    }

    unsigned long pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return false;
    }

    const std::wstring title = query_window_title(hwnd);
    const std::wstring path = query_process_path(pid);
    const std::string exe_path = wide_to_utf8(path);

    if (pid == last_pid_ && title == last_title_ && exe_path == last_exe_path_) {
        return false;
    }

    last_pid_ = pid;
    last_title_ = title;
    last_exe_path_ = exe_path;

    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 1;
    event.cat = "window";
    event.act = "focus";
    event.sev = "info";
    event.host = hostname_;
    event.src = "user32";
    event.data["pid"] = std::to_string(pid);

    if (!exe_path.empty()) {
        event.data["path"] = exe_path;
    }
    if (!title.empty()) {
        event.data["title"] = wide_to_utf8(title);
    }

    return writer_.write(event);
}

void ForegroundCollector::poll_thread_main() {
    while (running_.load()) {
        if (stop_flag_ != nullptr && *stop_flag_) {
            break;
        }

        emit_focus_if_changed();

        for (int i = 0; i < poll_interval_sec_ * 10; ++i) {
            if (!running_.load() || (stop_flag_ != nullptr && *stop_flag_)) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool ForegroundCollector::start() {
    if (running_.load()) {
        return true;
    }

    running_.store(true);
    poll_thread_ = std::thread([this]() { poll_thread_main(); });
    return true;
}

void ForegroundCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
}

}  // namespace useraudit
