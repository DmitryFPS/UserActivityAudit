#include "useraudit/user_agent_mode.hpp"

#include "useraudit/clipboard_collector.hpp"
#include "useraudit/config_manager.hpp"
#include "useraudit/foreground_collector.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/pipe_client.hpp"

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

}  // namespace

int run_user_agent_mode(int argc, wchar_t** argv) {
    const unsigned long session_id = parse_session_id(argc, argv);

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

    ClipboardCollector clipboard(pipe, hostname, config.clipboard_poll_sec());
    if (enable_clipboard) {
        clipboard.start();
    }

#if defined(USERAUDIT_DEV_CONSOLE)
    std::wcout << L"[UserAudit] User-agent session " << session_id << L" active.\n";
#endif

    volatile bool stop = false;
    foreground.set_stop_flag(&stop);
    if (enable_clipboard) {
        clipboard.set_stop_flag(&stop);
    }

    while (!stop) {
        Sleep(500);
    }

    clipboard.stop();
    foreground.stop();
    pipe.disconnect();
    return 0;
}

}  // namespace useraudit
