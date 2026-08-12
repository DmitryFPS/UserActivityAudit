#include "useraudit/event_loop.hpp"

#include "useraudit/encrypted_log_writer.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/pipe_server.hpp"
#include "useraudit/process_collector.hpp"
#include "useraudit/session_agent_manager.hpp"
#include "useraudit/session_collector.hpp"
#include "useraudit/usb_collector.hpp"

#include <windows.h>

#include <chrono>
#include <iostream>
#include <thread>

namespace useraudit {

void run_event_loop(volatile bool& stop_requested) {
    const std::string hostname = get_hostname_utf8();
    EncryptedLogWriter writer(resolve_log_directory(), hostname);
    if (!writer.initialize()) {
        OutputDebugStringW(L"[UserAuditSvc] EncryptedLogWriter failed to initialize keys\n");
    }

    PipeIngestServer pipe_server(writer);
    SessionAgentManager session_agents;

#if defined(USERAUDIT_DEV_CONSOLE)
    session_agents.set_dev_mode(true);
    std::wcout << L"[UserAuditSvc] Log directory: " << writer.log_directory().wstring() << L"\n";
#endif

    if (!pipe_server.start()) {
        OutputDebugStringW(L"[UserAuditSvc] PipeIngestServer failed to start\n");
    }

    if (!session_agents.start()) {
        OutputDebugStringW(L"[UserAuditSvc] SessionAgentManager failed to start\n");
    }

    SessionCollector session_collector(writer, hostname);
    ProcessCollector process_collector(writer, hostname);
    UsbCollector usb_collector(writer, hostname);

    session_collector.set_stop_flag(&stop_requested);
    process_collector.set_stop_flag(&stop_requested);
    usb_collector.set_stop_flag(&stop_requested);
    session_collector.set_session_observer(
        [&session_agents](const std::string& action, const std::string& user) {
            session_agents.on_session_event(action, user);
        });

    if (!session_collector.start()) {
#if defined(USERAUDIT_DEV_CONSOLE)
        std::wcerr << L"[UserAuditSvc] SessionCollector failed — run as Service (LocalSystem) "
                      L"or admin. See docs/SETUP.md\n";
#endif
        OutputDebugStringW(L"[UserAuditSvc] SessionCollector failed to start\n");
    }

    if (!process_collector.start()) {
        OutputDebugStringW(L"[UserAuditSvc] ProcessCollector failed to start\n");
    }

    if (!usb_collector.start()) {
        OutputDebugStringW(L"[UserAuditSvc] UsbCollector failed to start\n");
    }

    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    process_collector.stop();
    usb_collector.stop();
    session_collector.stop();
    session_agents.stop();
    pipe_server.stop();
}

}  // namespace useraudit
