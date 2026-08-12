#pragma once

#include "useraudit/jsonl_writer.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace useraudit {

class UsbCollector {
public:
    UsbCollector(JsonlWriter& writer, std::string hostname);
    ~UsbCollector();

    UsbCollector(const UsbCollector&) = delete;
    UsbCollector& operator=(const UsbCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    void wmi_thread_main();

    JsonlWriter& writer_;
    std::string hostname_;
    std::thread wmi_thread_;
    volatile bool* stop_flag_ = nullptr;
    std::atomic<bool> running_{false};
};

}  // namespace useraudit
