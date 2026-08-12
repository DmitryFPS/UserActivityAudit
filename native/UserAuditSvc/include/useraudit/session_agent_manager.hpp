#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <windows.h>

namespace useraudit {

// Launches UserAudit.exe --user-agent in interactive user sessions.
class SessionAgentManager {
public:
    using SessionHook = std::function<void(const std::string& action, const std::string& user)>;

    SessionAgentManager();
    ~SessionAgentManager();

    SessionAgentManager(const SessionAgentManager&) = delete;
    SessionAgentManager& operator=(const SessionAgentManager&) = delete;

    void set_dev_mode(bool dev_mode) { dev_mode_ = dev_mode; }
    void set_agent_options(int window_poll_sec, bool enable_clipboard);

    bool start();
    void stop();

    void on_session_event(const std::string& action, const std::string& user);

private:
    struct AgentRecord {
        unsigned long session_id = 0;
        std::string user;
        HANDLE process = nullptr;
    };

    void sync_active_sessions();
    void watchdog_loop();
    bool ensure_agent_for_session(unsigned long session_id, const std::string& user);
    void stop_agent_for_session(unsigned long session_id);
    void stop_agent_for_user(const std::string& user);
    bool launch_agent(unsigned long session_id, HANDLE& out_process);
    std::wstring resolve_user_agent_path() const;
    unsigned long find_session_for_user(const std::string& domain_user) const;

    bool dev_mode_ = false;
    int window_poll_sec_ = 3;
    bool enable_clipboard_ = false;
    std::mutex mutex_;
    std::unordered_map<unsigned long, AgentRecord> agents_;
    std::thread watchdog_thread_;
    HANDLE stop_event_ = nullptr;
    std::atomic<bool> running_{false};
};

}  // namespace useraudit
