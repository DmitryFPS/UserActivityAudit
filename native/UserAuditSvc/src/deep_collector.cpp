#include "useraudit/deep_collector.hpp"

#include "useraudit/evidence_pack.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/time_utils.hpp"

#include <windows.h>

namespace useraudit {

DeepCollector::DeepCollector(EventSink& writer, std::string hostname, const ConfigManager& config)
    : writer_(writer),
      hostname_(std::move(hostname)),
      settings_(config.raw_config().forensic),
      schedule_hours_(config.raw_config().forensic.schedule_hours) {}

DeepCollector::~DeepCollector() {
    stop();
}

bool DeepCollector::start() {
    if (running_.load()) {
        return true;
    }
    if (!settings_.enabled) {
        return false;
    }

    running_.store(true);
    last_scheduled_run_ = std::chrono::steady_clock::now();
    worker_ = std::thread([this]() { worker_main(); });
    return true;
}

void DeepCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    queue_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void DeepCollector::trigger_usb(const AuditEvent& usb_event) {
    if (!settings_.enabled || !settings_.trigger_on_usb) {
        return;
    }

    TriggerRequest request;
    request.reason = "usb_insert";
    request.corr = usb_event.corr;
    request.usb_context = usb_event.data;

    {
        std::lock_guard lock(queue_mutex_);
        queue_.push_back(std::move(request));
    }
    queue_cv_.notify_one();
}

void DeepCollector::trigger_manual(const std::string& reason) {
    if (!settings_.enabled) {
        return;
    }

    TriggerRequest request;
    request.reason = reason.empty() ? "manual" : reason;

    {
        std::lock_guard lock(queue_mutex_);
        queue_.push_back(std::move(request));
    }
    queue_cv_.notify_one();
}

bool DeepCollector::schedule_due() const {
    if (schedule_hours_ <= 0) {
        return false;
    }
    const auto elapsed = std::chrono::steady_clock::now() - last_scheduled_run_;
    return elapsed >= std::chrono::hours(schedule_hours_);
}

void DeepCollector::worker_main() {
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);

    while (running_.load()) {
        if (stop_flag_ != nullptr && *stop_flag_) {
            break;
        }

        if (schedule_due()) {
            TriggerRequest scheduled;
            scheduled.reason = "schedule";
            {
                std::lock_guard lock(queue_mutex_);
                queue_.push_back(scheduled);
            }
            last_scheduled_run_ = std::chrono::steady_clock::now();
        }

        TriggerRequest request;
        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
                return !queue_.empty() || !running_.load() ||
                       (stop_flag_ != nullptr && *stop_flag_);
            });

            if (!running_.load() || (stop_flag_ != nullptr && *stop_flag_)) {
                break;
            }
            if (queue_.empty()) {
                continue;
            }

            request = std::move(queue_.front());
            queue_.pop_front();
        }

        run_collection(request);
    }

    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    running_.store(false);
}

void DeepCollector::emit_browser_summary(int history_rows, int download_rows, const std::string& corr) {
    if (history_rows == 0 && download_rows == 0) {
        return;
    }

    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 3;
    event.cat = "browser";
    event.act = "history_collected";
    event.sev = "info";
    event.host = hostname_;
    event.src = "sqlite";
    event.corr = corr;
    event.data["history_rows"] = std::to_string(history_rows);
    event.data["download_rows"] = std::to_string(download_rows);
    writer_.write(event);
}

void DeepCollector::emit_pack_event(const EvidencePackResult& pack, const TriggerRequest& request) {
    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 3;
    event.cat = "forensic";
    event.act = "pack_created";
    event.sev = "info";
    event.host = hostname_;
    event.src = "evidence_pack";
    event.corr = request.corr;
    event.data["trigger"] = request.reason;
    event.data["pack_path"] = pack.zip_path.string();
    event.data["artifact_files"] = std::to_string(pack.artifact_files);
    event.data["browser_history_rows"] = std::to_string(pack.browser_history_rows);
    event.data["browser_download_rows"] = std::to_string(pack.browser_download_rows);
    event.data["prefetch_files"] = std::to_string(pack.prefetch_files);
    event.data["usb_registry_lines"] = std::to_string(pack.usb_registry_lines);
    writer_.write(event);
}

void DeepCollector::run_collection(const TriggerRequest& request) {
    EvidencePackRequest pack_request;
    pack_request.trigger = request.reason;
    pack_request.corr = request.corr;
    pack_request.usb_context = request.usb_context;

    std::string error;
    const auto pack = build_evidence_pack(settings_, pack_request, resolve_forensic_staging_directory(),
                                          resolve_packs_directory(), error);
    if (pack.zip_path.empty()) {
        AuditEvent fail;
        fail.id = generate_event_id();
        fail.ts = utc_now_iso8601();
        fail.lvl = 3;
        fail.cat = "forensic";
        fail.act = "pack_failed";
        fail.sev = "warning";
        fail.host = hostname_;
        fail.src = "evidence_pack";
        fail.corr = request.corr;
        fail.data["trigger"] = request.reason;
        fail.data["error"] = error.empty() ? "unknown" : error;
        writer_.write(fail);
        return;
    }

    emit_browser_summary(pack.browser_history_rows, pack.browser_download_rows, request.corr);
    emit_pack_event(pack, request);
}

}  // namespace useraudit
