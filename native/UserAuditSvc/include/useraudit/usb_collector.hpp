#pragma once

#include "useraudit/audit_event.hpp"
#include "useraudit/correlation_tracker.hpp"
#include "useraudit/event_sink.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace useraudit {

class UsbCollector {
public:
    using InsertObserver = std::function<void(const AuditEvent&)>;

    UsbCollector(EventSink& writer, std::string hostname);
    ~UsbCollector();

    UsbCollector(const UsbCollector&) = delete;
    UsbCollector& operator=(const UsbCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    void set_correlation_tracker(CorrelationTracker* tracker) { correlation_ = tracker; }

    void set_insert_observer(InsertObserver observer) { insert_observer_ = std::move(observer); }

    bool start();
    void stop();

private:
    void wmi_thread_main();

    EventSink& writer_;
    std::string hostname_;
    CorrelationTracker* correlation_ = nullptr;
    InsertObserver insert_observer_;
    std::thread wmi_thread_;
    volatile bool* stop_flag_ = nullptr;
    std::atomic<bool> running_{false};
};

}  // namespace useraudit
