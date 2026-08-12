#include "useraudit/event_loop.hpp"

#include "useraudit/acl_guard.hpp"
#include "useraudit/config_manager.hpp"
#include "useraudit/correlation_tracker.hpp"
#include "useraudit/encrypted_log_writer.hpp"
#include "useraudit/file_collector.hpp"
#include "useraudit/network_collector.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/pipe_server.hpp"
#include "useraudit/print_collector.hpp"
#include "useraudit/process_collector.hpp"
#include "useraudit/session_agent_manager.hpp"
#include "useraudit/session_collector.hpp"
#include "useraudit/tamper_collector.hpp"
#include "useraudit/upload_client.hpp"
#include "useraudit/usb_collector.hpp"

#include <windows.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace useraudit {

void run_event_loop(volatile bool& stop_requested) {
    ConfigManager config;
    if (!config.load()) {
        OutputDebugStringW(L"[UserAuditSvc] ConfigManager failed to load config.json\n");
    }

    const std::string hostname = get_hostname_utf8();
    const std::uint64_t max_log_bytes = config.max_log_mb_per_day() * 1024ULL * 1024ULL;
    EncryptedLogWriter writer(resolve_log_directory(), hostname, max_log_bytes);
    if (!writer.initialize()) {
        OutputDebugStringW(L"[UserAuditSvc] EncryptedLogWriter failed to initialize keys\n");
    }

    CorrelationTracker correlation;
    PipeIngestServer pipe_server(writer);
    SessionAgentManager session_agents;
    session_agents.set_agent_options(config.window_poll_sec(), config.collector_enabled("clipboard"));

#if defined(USERAUDIT_DEV_CONSOLE)
    session_agents.set_dev_mode(true);
    std::wcout << L"[UserAuditSvc] Log directory: " << writer.log_directory().wstring() << L"\n";
    std::wcout << L"[UserAuditSvc] Config: " << resolve_config_path().wstring() << L"\n";
#endif

    if (!pipe_server.start()) {
        OutputDebugStringW(L"[UserAuditSvc] PipeIngestServer failed to start\n");
    }

    if (!session_agents.start()) {
        OutputDebugStringW(L"[UserAuditSvc] SessionAgentManager failed to start\n");
    }

    std::unique_ptr<SessionCollector> session_collector;
    std::unique_ptr<ProcessCollector> process_collector;
    std::unique_ptr<UsbCollector> usb_collector;
    std::unique_ptr<FileCollector> file_collector;
    std::unique_ptr<NetworkCollector> network_collector;
    std::unique_ptr<PrintCollector> print_collector;

    if (config.collector_enabled("session")) {
        session_collector = std::make_unique<SessionCollector>(writer, hostname);
        session_collector->set_stop_flag(&stop_requested);
        session_collector->set_session_observer(
            [&session_agents](const std::string& action, const std::string& user) {
                session_agents.on_session_event(action, user);
            });
        if (!session_collector->start()) {
#if defined(USERAUDIT_DEV_CONSOLE)
            std::wcerr << L"[UserAuditSvc] SessionCollector failed — run as Service (LocalSystem) "
                          L"or admin. See docs/SETUP.md\n";
#endif
            OutputDebugStringW(L"[UserAuditSvc] SessionCollector failed to start\n");
        }
    }

    if (config.collector_enabled("process")) {
        process_collector = std::make_unique<ProcessCollector>(writer, hostname);
        process_collector->set_stop_flag(&stop_requested);
        if (!process_collector->start()) {
            OutputDebugStringW(L"[UserAuditSvc] ProcessCollector failed to start\n");
        }
    }

    if (config.collector_enabled("usb")) {
        usb_collector = std::make_unique<UsbCollector>(writer, hostname);
        usb_collector->set_stop_flag(&stop_requested);
        usb_collector->set_correlation_tracker(&correlation);
        if (!usb_collector->start()) {
            OutputDebugStringW(L"[UserAuditSvc] UsbCollector failed to start\n");
        }
    }

    if (config.collector_enabled("file")) {
        file_collector = std::make_unique<FileCollector>(writer, hostname, config, correlation);
        file_collector->set_stop_flag(&stop_requested);
        if (!file_collector->start()) {
            OutputDebugStringW(L"[UserAuditSvc] FileCollector failed to start\n");
        }
    }

    if (config.collector_enabled("network")) {
        network_collector = std::make_unique<NetworkCollector>(writer, hostname, config);
        network_collector->set_stop_flag(&stop_requested);
        if (!network_collector->start()) {
            OutputDebugStringW(L"[UserAuditSvc] NetworkCollector failed to start\n");
        }
    }

    if (config.collector_enabled("print")) {
        print_collector = std::make_unique<PrintCollector>(writer, hostname);
        print_collector->set_stop_flag(&stop_requested);
        if (!print_collector->start()) {
            OutputDebugStringW(L"[UserAuditSvc] PrintCollector failed to start\n");
        }
    }

    AclGuard acl_guard(writer, hostname);
    acl_guard.set_stop_flag(&stop_requested);
    if (!acl_guard.start()) {
        OutputDebugStringW(L"[UserAuditSvc] AclGuard failed to start\n");
    }

    TamperCollector tamper_collector(writer, hostname);
    tamper_collector.set_stop_flag(&stop_requested);
    if (!tamper_collector.start()) {
        OutputDebugStringW(L"[UserAuditSvc] TamperCollector failed to start\n");
    }

    UploadClient upload_client(writer.log_directory(), parse_upload_settings(config.raw_config()));
    upload_client.set_stop_flag(&stop_requested);
    if (!upload_client.start()) {
        OutputDebugStringW(L"[UserAuditSvc] UploadClient failed to start\n");
    }

    auto last_config_check = std::chrono::steady_clock::now();
    while (!stop_requested) {
        const auto now = std::chrono::steady_clock::now();
        if (now - last_config_check >= std::chrono::seconds(60)) {
            config.reload_if_modified();
            last_config_check = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (network_collector) {
        network_collector->stop();
    }
    upload_client.stop();
    tamper_collector.stop();
    acl_guard.stop();
    if (file_collector) {
        file_collector->stop();
    }
    if (print_collector) {
        print_collector->stop();
    }
    if (process_collector) {
        process_collector->stop();
    }
    if (usb_collector) {
        usb_collector->stop();
    }
    if (session_collector) {
        session_collector->stop();
    }
    session_agents.stop();
    pipe_server.stop();
}

}  // namespace useraudit
