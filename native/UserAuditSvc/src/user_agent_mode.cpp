#include "useraudit/user_agent_mode.hpp"

#include "useraudit/clipboard_collector.hpp"
#include "useraudit/config_manager.hpp"
#include "useraudit/foreground_collector.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/pipe_client.hpp"
#include "useraudit/session_notification_collector.hpp"

#include <windows.h>
#include <wtsapi32.h>

#include <iostream>
#include <string>

namespace useraudit {

namespace {

unsigned long parse_session_id(int argc, wchar_t** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::wstring(argv[i]) == L"--session-id") {
            return static_cast<unsigned long>(_wtoi(argv[i + 1]));
        }
    }
    return WTSGetActiveConsoleSessionId();
}

int parse_window_poll_sec(int argc, wchar_t** argv, int fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::wstring(argv[i]) == L"--window-poll-sec") {
            const int value = _wtoi(argv[i + 1]);
            return value > 0 ? value : fallback;
        }
    }
    return fallback;
}

bool has_flag(int argc, wchar_t** argv, const wchar_t* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::wstring(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

bool attach_process_to_interactive_window_station() {
    HWINSTA window_station =
        OpenWindowStationW(L"winsta0", FALSE, WINSTA_ENUMERATE | WINSTA_READATTRIBUTES);
    if (window_station == nullptr) {
        return false;
    }

    if (!SetProcessWindowStation(window_station)) {
        CloseWindowStation(window_station);
        return false;
    }

    CloseWindowStation(window_station);
    return true;
}

}  // namespace

int run_user_agent_mode(int argc, wchar_t** argv) {
    const unsigned long session_id = parse_session_id(argc, argv);

    if (!attach_process_to_interactive_window_station()) {
        OutputDebugStringW(L"[UserAudit] User-agent failed to attach to winsta0.\n");
    }

    ConfigManager config;
    config.load();
    const int window_poll_sec = parse_window_poll_sec(argc, argv, config.window_poll_sec());
    const bool enable_clipboard =
        has_flag(argc, argv, L"--enable-clipboard") || config.collector_enabled("clipboard");

    const std::string hostname = get_hostname_utf8();

    PipeClient pipe;
    if (!pipe.connect()) {
        OutputDebugStringW(L"[UserAudit] User-agent failed to connect to service pipe.\n");
        return 1;
    }

    ForegroundCollector foreground(pipe, hostname, session_id, window_poll_sec);
    if (!foreground.start()) {
        return 1;
    }

    SessionNotificationCollector session_notify(pipe, hostname, session_id);
    if (!session_notify.start()) {
        OutputDebugStringW(L"[UserAudit] SessionNotificationCollector failed to start.\n");
    }

    ClipboardCollector clipboard(pipe, hostname, config.clipboard_poll_sec());
    if (enable_clipboard) {
        clipboard.start();
    }

#if defined(USERAUDIT_DEV_CONSOLE)
    std::wcout << L"[UserAudit] User-agent session " << session_id << L" active.\n";
#endif

    volatile bool stop = false;
    foreground.set_stop_flag(&stop);
    session_notify.set_stop_flag(&stop);
    if (enable_clipboard) {
        clipboard.set_stop_flag(&stop);
    }

    while (!stop) {
        Sleep(500);
    }

    clipboard.stop();
    session_notify.stop();
    foreground.stop();
    pipe.disconnect();
    return 0;
}

}  // namespace useraudit
