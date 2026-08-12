#pragma once

#include "useraudit/event_sink.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace useraudit {

class ForegroundCollector {
public:
    ForegroundCollector(EventSink& sink, std::string hostname, unsigned long session_id = 0,
                        int poll_interval_sec = 5);
    ~ForegroundCollector();

    ForegroundCollector(const ForegroundCollector&) = delete;
    ForegroundCollector& operator=(const ForegroundCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    void poll_thread_main();
    bool emit_focus_if_changed();

    EventSink& sink_;
    std::string hostname_;
    unsigned long session_id_;
    int poll_interval_sec_;
    std::thread poll_thread_;
    volatile bool* stop_flag_ = nullptr;
    std::atomic<bool> running_{false};

    unsigned long last_pid_ = 0;
    std::wstring last_title_;
    std::string last_exe_path_;
};

}  // namespace useraudit
