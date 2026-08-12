#pragma once

#include "useraudit/jsonl_writer.hpp"

#include <functional>
#include <windows.h>
#include <winevt.h>

namespace useraudit {

class SessionCollector {
public:
    using SessionObserver = std::function<void(const std::string& action, const std::string& user)>;

    SessionCollector(JsonlWriter& writer, std::string hostname);
    ~SessionCollector();

    SessionCollector(const SessionCollector&) = delete;
    SessionCollector& operator=(const SessionCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }
    void set_session_observer(SessionObserver observer) { session_observer_ = std::move(observer); }

    bool start();
    void stop();

private:
    static DWORD WINAPI notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context,
                                        EVT_HANDLE event_record);

    bool handle_event(EVT_HANDLE event_record);

    JsonlWriter& writer_;
    std::string hostname_;
    SessionObserver session_observer_;
    EVT_HANDLE subscription_ = nullptr;
    volatile bool* stop_flag_ = nullptr;
};

}  // namespace useraudit
