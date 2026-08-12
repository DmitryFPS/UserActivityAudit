#include "useraudit/event_loop.hpp"

#include "useraudit/jsonl_writer.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/session_collector.hpp"

#include <windows.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace useraudit {

void run_event_loop(volatile bool& stop_requested) {
    const std::string hostname = get_hostname_utf8();
    JsonlWriter writer(resolve_log_directory(), hostname);
    SessionCollector session_collector(writer, hostname);
    session_collector.set_stop_flag(&stop_requested);

    if (!session_collector.start()) {
#if defined(USERAUDIT_DEV_CONSOLE)
        std::wcerr << L"[UserAuditSvc] SessionCollector failed to start. Security log access "
                      L"requires LocalSystem or SeSecurityPrivilege.\n";
#endif
        OutputDebugStringW(L"[UserAuditSvc] SessionCollector failed to start\n");
    } else {
#if defined(USERAUDIT_DEV_CONSOLE)
        std::wcout << L"[UserAuditSvc] SessionCollector active. Logs: "
                   << writer.log_directory().wstring() << L"\n";
#endif
    }

    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    session_collector.stop();
}

}  // namespace useraudit
