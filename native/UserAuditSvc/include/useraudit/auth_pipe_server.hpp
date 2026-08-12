#pragma once

#include "useraudit/auth_guard.hpp"

#include <atomic>
#include <thread>

namespace useraudit {

// Named pipe \\.\pipe\UserAudit\Auth — challenge/token exchange for IT USB ceremony.
class AuthPipeServer {
public:
    explicit AuthPipeServer(AuthGuard& guard);
    ~AuthPipeServer();

    AuthPipeServer(const AuthPipeServer&) = delete;
    AuthPipeServer& operator=(const AuthPipeServer&) = delete;

    void set_stop_flag(volatile bool* flag) { stop_flag_ = flag; }

    bool start();
    void stop();

private:
    void server_thread_main();
    bool handle_client(void* pipe_handle);

    AuthGuard& guard_;
    volatile bool* stop_flag_ = nullptr;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
};

}  // namespace useraudit
