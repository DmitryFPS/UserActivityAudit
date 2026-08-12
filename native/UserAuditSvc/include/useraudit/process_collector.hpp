#pragma once

#include "useraudit/event_sink.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace useraudit {

class ProcessCollector {
public:
    ProcessCollector(EventSink& writer, std::string hostname);
    ~ProcessCollector();

    ProcessCollector(const ProcessCollector&) = delete;
    ProcessCollector& operator=(const ProcessCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    void trace_thread_main();

    EventSink& writer_;
    std::string hostname_;
    std::thread trace_thread_;
    volatile bool* stop_flag_ = nullptr;
    std::atomic<bool> running_{false};
    std::uint64_t session_id_value_{0};
    unsigned long long trace_handle_ = ~0ULL;
};

}  // namespace useraudit
