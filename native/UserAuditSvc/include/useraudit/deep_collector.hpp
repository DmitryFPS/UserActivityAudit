#pragma once

#include "useraudit/audit_event.hpp"
#include "useraudit/config_manager.hpp"
#include "useraudit/evidence_pack.hpp"
#include "useraudit/event_sink.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace useraudit {

class DeepCollector {
public:
    DeepCollector(EventSink& writer, std::string hostname, const ConfigManager& config);
    ~DeepCollector();

    DeepCollector(const DeepCollector&) = delete;
    DeepCollector& operator=(const DeepCollector&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

    void trigger_usb(const AuditEvent& usb_event);
    void trigger_manual(const std::string& reason);

private:
    struct TriggerRequest {
        std::string reason;
        std::string corr;
        std::map<std::string, std::string> usb_context;
    };

    void worker_main();
    void run_collection(const TriggerRequest& request);
    void emit_pack_event(const EvidencePackResult& pack, const TriggerRequest& request);
    void emit_browser_summary(int history_rows, int download_rows, const std::string& corr);
    bool schedule_due() const;

    EventSink& writer_;
    std::string hostname_;
    ForensicSettings settings_;
    int schedule_hours_ = 0;

    std::thread worker_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<TriggerRequest> queue_;
    std::atomic<bool> running_{false};
    volatile bool* stop_flag_ = nullptr;
    std::chrono::steady_clock::time_point last_scheduled_run_{};
};

}  // namespace useraudit
