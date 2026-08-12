#pragma once

#include "useraudit/event_sink.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <windows.h>
#include <winevt.h>

namespace useraudit {

class EventForwarder;
class LockdownManager;

// Security/System event subscriptions for tamper attempts (service stop, protected paths).
class TamperCollector {
public:
    TamperCollector(EventSink& writer, std::string hostname);
    ~TamperCollector();

    TamperCollector(const TamperCollector&) = delete;
    TamperCollector& operator=(const TamperCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }
    void set_lockdown_manager(LockdownManager* manager) { lockdown_manager_ = manager; }
    void set_event_forwarder(EventForwarder* forwarder) { event_forwarder_ = forwarder; }

    bool start();
    void stop();

private:
    static DWORD WINAPI system_notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context,
                                               EVT_HANDLE event_record);
    static DWORD WINAPI security_notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context,
                                                 EVT_HANDLE event_record);

    bool handle_system_event(EVT_HANDLE event_record);
    bool handle_security_event(EVT_HANDLE event_record);
    void emit_tamper(const std::string& act, const std::string& detail);

    EventSink& writer_;
    std::string hostname_;
    EVT_HANDLE system_subscription_ = nullptr;
    EVT_HANDLE security_subscription_ = nullptr;
    volatile bool* stop_flag_ = nullptr;
    LockdownManager* lockdown_manager_ = nullptr;
    EventForwarder* event_forwarder_ = nullptr;
};

}  // namespace useraudit
