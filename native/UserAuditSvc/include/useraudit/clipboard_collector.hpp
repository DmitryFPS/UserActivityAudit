#pragma once

#include "useraudit/event_sink.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace useraudit {

class ClipboardCollector {
public:
    ClipboardCollector(EventSink& writer, std::string hostname, int poll_interval_sec);
    ~ClipboardCollector();

    ClipboardCollector(const ClipboardCollector&) = delete;
    ClipboardCollector& operator=(const ClipboardCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    void poll_thread_main();
    bool emit_if_changed();

    EventSink& writer_;
    std::string hostname_;
    int poll_interval_sec_;
    std::thread poll_thread_;
    std::atomic<bool> running_{false};
    volatile bool* stop_flag_ = nullptr;
    std::string last_hash_;
};

}  // namespace useraudit
