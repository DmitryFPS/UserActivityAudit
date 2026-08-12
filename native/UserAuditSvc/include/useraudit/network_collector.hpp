#pragma once

#include "useraudit/config_manager.hpp"
#include "useraudit/event_sink.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace useraudit {

class NetworkCollector {
public:
    NetworkCollector(EventSink& writer, std::string hostname, const ConfigManager& config);

    NetworkCollector(const NetworkCollector&) = delete;
    NetworkCollector& operator=(const NetworkCollector&) = delete;

    ~NetworkCollector();

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    void poll_thread_main();
    void emit_snapshot();

    EventSink& writer_;
    std::string hostname_;
    const ConfigManager& config_;
    std::thread poll_thread_;
    std::atomic<bool> running_{false};
    volatile bool* stop_flag_ = nullptr;
};

}  // namespace useraudit
