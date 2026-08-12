#pragma once

#include "useraudit/config_manager.hpp"
#include "useraudit/correlation_tracker.hpp"
#include "useraudit/event_sink.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

namespace useraudit {

class FileCollector {
public:
    FileCollector(EventSink& writer, std::string hostname, const ConfigManager& config,
                  CorrelationTracker& correlation);

    FileCollector(const FileCollector&) = delete;
    FileCollector& operator=(const FileCollector&) = delete;

    ~FileCollector();

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    void poll_thread_main();
    void scan_root(const std::wstring& root, const std::string& correlation, bool removable);
    void emit_file_create(const std::wstring& path, const std::string& correlation, bool removable);

    EventSink& writer_;
    std::string hostname_;
    const ConfigManager& config_;
    CorrelationTracker& correlation_;
    std::thread poll_thread_;
    std::atomic<bool> running_{false};
    volatile bool* stop_flag_ = nullptr;
    std::mutex state_mutex_;
    std::unordered_set<std::string> known_files_;
    bool baseline_ready_ = false;
};

}  // namespace useraudit
