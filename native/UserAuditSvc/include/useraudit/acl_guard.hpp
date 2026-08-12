#pragma once

#include "useraudit/event_sink.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

namespace useraudit {

// Applies and periodically verifies restrictive ACL on %ProgramData%\\UserAudit\\.
class AclGuard {
public:
    AclGuard(EventSink& writer, std::string hostname);
    ~AclGuard();

    AclGuard(const AclGuard&) = delete;
    AclGuard& operator=(const AclGuard&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool apply();
    bool verify() const;

    bool start();
    void stop();

private:
    void guard_thread_main();
    bool apply_to_path(const std::filesystem::path& path, bool is_directory) const;
    void emit_tamper_event(const std::string& reason) const;

    EventSink& writer_;
    std::string hostname_;
    volatile bool* stop_flag_ = nullptr;
    std::thread guard_thread_;
    std::atomic<bool> running_{false};
};

// Returns SDDL fragment used for verification (unit-testable).
std::wstring expected_users_deny_sddl_fragment();

}  // namespace useraudit
