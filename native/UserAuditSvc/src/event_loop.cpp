#include "useraudit/event_loop.hpp"

#include "useraudit/foreground_collector.hpp"
#include "useraudit/jsonl_writer.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/process_collector.hpp"
#include "useraudit/session_collector.hpp"

#include <windows.h>

#include <chrono>
#include <iostream>
#include <thread>

namespace useraudit {

void run_event_loop(volatile bool& stop_requested) {
    const std::string hostname = get_hostname_utf8();
    JsonlWriter writer(resolve_log_directory(), hostname);

    SessionCollector session_collector(writer, hostname);
    ProcessCollector process_collector(writer, hostname);
    ForegroundCollector foreground_collector(writer, hostname, 5);

    session_collector.set_stop_flag(&stop_requested);
    process_collector.set_stop_flag(&stop_requested);
    foreground_collector.set_stop_flag(&stop_requested);

#if defined(USERAUDIT_DEV_CONSOLE)
    std::wcout << L"[UserAuditSvc] Log directory: " << writer.log_directory().wstring() << L"\n";
#endif

    if (!session_collector.start()) {
#if defined(USERAUDIT_DEV_CONSOLE)
        std::wcerr << L"[UserAuditSvc] SessionCollector failed — install/start as Windows Service "
                      L"(LocalSystem). See docs/SETUP.md\n";
#endif
        OutputDebugStringW(L"[UserAuditSvc] SessionCollector failed to start\n");
    }

    if (!process_collector.start()) {
        OutputDebugStringW(L"[UserAuditSvc] ProcessCollector failed to start\n");
    }

    if (!foreground_collector.start()) {
        OutputDebugStringW(L"[UserAuditSvc] ForegroundCollector failed to start\n");
    }

    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    foreground_collector.stop();
    process_collector.stop();
    session_collector.stop();
}

}  // namespace useraudit
