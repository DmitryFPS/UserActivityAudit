#include "useraudit/user_agent_mode.hpp"

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

}  // namespace

int run_user_agent_mode(int argc, wchar_t** argv) {
    const unsigned long session_id = parse_session_id(argc, argv);
    const std::string hostname = get_hostname_utf8();

    PipeClient pipe;
    if (!pipe.connect()) {
        OutputDebugStringW(L"[UserAudit] User-agent failed to connect to service pipe.\n");
        return 1;
    }

    ForegroundCollector foreground(pipe, hostname, session_id, 5);
    if (!foreground.start()) {
        return 1;
    }

#if defined(USERAUDIT_DEV_CONSOLE)
    std::wcout << L"[UserAudit] User-agent session " << session_id << L" active.\n";
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

}  // namespace useraudit
