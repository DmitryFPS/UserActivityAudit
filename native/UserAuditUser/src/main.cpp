#include "useraudit/foreground_collector.hpp"
#include "useraudit/pipe_client.hpp"
#include "useraudit/paths.hpp"

#include <windows.h>

#include <iostream>
#include <string>

namespace {

unsigned long parse_session_id(int argc, wchar_t** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::wstring(argv[i]) == L"--session-id") {
            return static_cast<unsigned long>(_wtoi(argv[i + 1]));
        }
    }
    return WTSGetActiveConsoleSessionId();
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const unsigned long session_id = parse_session_id(argc, argv);
    const std::string hostname = useraudit::get_hostname_utf8();

    useraudit::PipeClient pipe;
    if (!pipe.connect()) {
        std::wcerr << L"[UserAuditUser] Failed to connect to UserAuditSvc pipe.\n";
        return 1;
    }

    useraudit::ForegroundCollector foreground(pipe, hostname, session_id, 5);
    if (!foreground.start()) {
        std::wcerr << L"[UserAuditUser] ForegroundCollector failed to start.\n";
        return 1;
    }

#if defined(USERAUDIT_DEV_CONSOLE)
    std::wcout << L"[UserAuditUser] Session " << session_id << L" foreground tracking active.\n";
#endif

    volatile bool stop = false;
    foreground.set_stop_flag(&stop);

    while (!stop) {
        Sleep(500);
    }

    foreground.stop();
    pipe.disconnect();
    return 0;
}
