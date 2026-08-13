#pragma once

#include "useraudit/event_sink.hpp"

#include <windows.h>

#include <atomic>
#include <string>
#include <thread>

namespace useraudit {

// Lock/unlock via WTS session notifications (works without Security audit 4800/4801).
class SessionNotificationCollector final {
public:
    SessionNotificationCollector(EventSink& sink, std::string hostname, unsigned long session_id);
    ~SessionNotificationCollector();

    SessionNotificationCollector(const SessionNotificationCollector&) = delete;
    SessionNotificationCollector& operator=(const SessionNotificationCollector&) = delete;

    bool start();
    void stop();
    void set_stop_flag(volatile bool* flag);
    void on_wts_session_change(WPARAM event_type);

private:
    void message_thread_main();
    void emit_session_event(const std::string& action);
    std::string resolve_session_user() const;

    EventSink& sink_;
    std::string hostname_;
    unsigned long session_id_;
    volatile bool* stop_flag_ = nullptr;

    std::atomic<bool> running_{false};
    std::thread message_thread_;
    HWND window_handle_ = nullptr;
};

}  // namespace useraudit
