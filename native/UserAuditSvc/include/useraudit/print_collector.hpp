#pragma once

#include "useraudit/event_sink.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <windows.h>
#include <winevt.h>

namespace useraudit {

class PrintCollector {
public:
    PrintCollector(EventSink& writer, std::string hostname);
    ~PrintCollector();

    PrintCollector(const PrintCollector&) = delete;
    PrintCollector& operator=(const PrintCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    static DWORD WINAPI notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context,
                                        EVT_HANDLE event_record);
    bool handle_event(EVT_HANDLE event_record);

    EventSink& writer_;
    std::string hostname_;
    EVT_HANDLE subscription_ = nullptr;
    volatile bool* stop_flag_ = nullptr;
};

}  // namespace useraudit
