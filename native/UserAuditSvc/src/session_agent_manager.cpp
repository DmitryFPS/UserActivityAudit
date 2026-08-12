#include "useraudit/session_agent_manager.hpp"

#include "useraudit/service_host.hpp"

#include <userenv.h>
#include <wtsapi32.h>

#include <thread>
#include <vector>

namespace useraudit {

namespace {

constexpr unsigned long kInvalidSession = 0xFFFFFFFF;

std::pair<std::wstring, std::wstring> split_domain_user(const std::string& domain_user) {
    const auto pos = domain_user.find('\\');
    if (pos == std::string::npos) {
        return {L"", std::wstring(domain_user.begin(), domain_user.end())};
    }
    return {std::wstring(domain_user.begin(), domain_user.begin() + static_cast<std::ptrdiff_t>(pos)),
            std::wstring(domain_user.begin() + static_cast<std::ptrdiff_t>(pos + 1),
                         domain_user.end())};
}

bool is_user_session(unsigned long session_id) {
    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id, WTSUserName, &buffer,
                                     &bytes)) {
        return false;
    }
    const bool has_user = buffer != nullptr && buffer[0] != L'\0';
    WTSFreeMemory(buffer);
    return has_user;
}

}  // namespace

SessionAgentManager::SessionAgentManager() = default;

void SessionAgentManager::set_agent_options(int window_poll_sec, bool enable_clipboard) {
    window_poll_sec_ = window_poll_sec > 0 ? window_poll_sec : 3;
    enable_clipboard_ = enable_clipboard;
}

SessionAgentManager::~SessionAgentManager() {
    stop();
}

bool SessionAgentManager::start() {
    if (running_.load()) {
        return true;
    }

    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stop_event_ == nullptr) {
        return false;
    }

    running_.store(true);
    sync_active_sessions();
    watchdog_thread_ = std::thread([this]() { watchdog_loop(); });
    return true;
}

void SessionAgentManager::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (stop_event_ != nullptr) {
        SetEvent(stop_event_);
    }

    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }

    std::lock_guard lock(mutex_);
    for (auto& [session_id, record] : agents_) {
        if (record.process != nullptr) {
            TerminateProcess(record.process, 0);
            WaitForSingleObject(record.process, 3000);
            CloseHandle(record.process);
            record.process = nullptr;
        }
        (void)session_id;
    }
    agents_.clear();

    if (stop_event_ != nullptr) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }
}

void SessionAgentManager::on_session_event(const std::string& action, const std::string& user) {
    if (user.empty()) {
        return;
    }

    if (action == "login" || action == "unlock") {
        const unsigned long session_id = find_session_for_user(user);
        if (session_id != kInvalidSession) {
            ensure_agent_for_session(session_id, user);
        } else {
            sync_active_sessions();
        }
        return;
    }

    if (action == "logout") {
        stop_agent_for_user(user);
    }
}

void SessionAgentManager::sync_active_sessions() {
    WTS_SESSION_INFOW* sessions = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
        return;
    }

    for (DWORD i = 0; i < count; ++i) {
        const unsigned long session_id = sessions[i].SessionId;
        if (sessions[i].State != WTSActive && sessions[i].State != WTSConnected) {
            continue;
        }
        if (!is_user_session(session_id)) {
            continue;
        }

        LPWSTR user_buffer = nullptr;
        DWORD bytes = 0;
        if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id, WTSUserName,
                                         &user_buffer, &bytes)) {
            continue;
        }

        std::wstring domain;
        LPWSTR domain_buffer = nullptr;
        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id, WTSDomainName,
                                        &domain_buffer, &bytes)) {
            domain = domain_buffer != nullptr ? domain_buffer : L"";
            WTSFreeMemory(domain_buffer);
        }

        std::string domain_user;
        if (!domain.empty()) {
            domain_user = std::string(domain.begin(), domain.end()) + "\\" +
                          std::string(user_buffer, user_buffer + wcslen(user_buffer));
        } else {
            domain_user = std::string(user_buffer, user_buffer + wcslen(user_buffer));
        }
        WTSFreeMemory(user_buffer);

        ensure_agent_for_session(session_id, domain_user);
    }

    WTSFreeMemory(sessions);
}

void SessionAgentManager::watchdog_loop() {
    while (running_.load()) {
        {
            std::lock_guard lock(mutex_);
            for (auto& [session_id, record] : agents_) {
                if (record.process == nullptr) {
                    continue;
                }
                if (WaitForSingleObject(record.process, 0) == WAIT_OBJECT_0) {
                    CloseHandle(record.process);
                    record.process = nullptr;
                }
                (void)session_id;
            }
        }

        sync_active_sessions();

        if (WaitForSingleObject(stop_event_, 30000) == WAIT_OBJECT_0) {
            break;
        }
    }
}

bool SessionAgentManager::ensure_agent_for_session(unsigned long session_id,
                                                   const std::string& user) {
    std::lock_guard lock(mutex_);

    auto it = agents_.find(session_id);
    if (it != agents_.end() && it->second.process != nullptr) {
        if (WaitForSingleObject(it->second.process, 0) == WAIT_TIMEOUT) {
            return true;
        }
        CloseHandle(it->second.process);
        it->second.process = nullptr;
    }

    HANDLE process = nullptr;
    if (!launch_agent(session_id, process)) {
        agents_[session_id] = AgentRecord{session_id, user, nullptr};
        return false;
    }

    agents_[session_id] = AgentRecord{session_id, user, process};
    return true;
}

void SessionAgentManager::stop_agent_for_session(unsigned long session_id) {
    std::lock_guard lock(mutex_);
    const auto it = agents_.find(session_id);
    if (it == agents_.end()) {
        return;
    }
    if (it->second.process != nullptr) {
        TerminateProcess(it->second.process, 0);
        WaitForSingleObject(it->second.process, 3000);
        CloseHandle(it->second.process);
    }
    agents_.erase(it);
}

void SessionAgentManager::stop_agent_for_user(const std::string& user) {
    std::lock_guard lock(mutex_);
    for (auto it = agents_.begin(); it != agents_.end();) {
        if (it->second.user == user) {
            if (it->second.process != nullptr) {
                TerminateProcess(it->second.process, 0);
                WaitForSingleObject(it->second.process, 3000);
                CloseHandle(it->second.process);
            }
            it = agents_.erase(it);
        } else {
            ++it;
        }
    }
}

std::wstring SessionAgentManager::resolve_user_agent_path() const {
    return get_module_path();
}

bool SessionAgentManager::launch_agent(unsigned long session_id, HANDLE& out_process) {
    out_process = nullptr;
    const std::wstring exe_path = resolve_user_agent_path();

    std::wstring command_line = L"\"";
    command_line += exe_path;
    command_line += L"\" --user-agent --session-id ";
    command_line += std::to_wstring(session_id);
    command_line += L" --window-poll-sec ";
    command_line += std::to_wstring(window_poll_sec_);
    if (enable_clipboard_) {
        command_line += L" --enable-clipboard";
    }
    std::vector<wchar_t> command_buffer(command_line.begin(), command_line.end());
    command_buffer.push_back(L'\0');

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(L"winsta0\\default");

    PROCESS_INFORMATION process_info{};

    if (dev_mode_) {
        if (!CreateProcessW(exe_path.c_str(), command_buffer.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr,
                            &startup_info, &process_info)) {
            return false;
        }
    } else {
        HANDLE user_token = nullptr;
        if (!WTSQueryUserToken(session_id, &user_token)) {
            return false;
        }

        LPVOID environment = nullptr;
        CreateEnvironmentBlock(&environment, user_token, FALSE);

        const BOOL created = CreateProcessAsUserW(
            user_token, exe_path.c_str(), command_buffer.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, environment, nullptr, &startup_info,
            &process_info);

        if (environment != nullptr) {
            DestroyEnvironmentBlock(environment);
        }
        CloseHandle(user_token);

        if (!created) {
            return false;
        }
    }

    CloseHandle(process_info.hThread);
    out_process = process_info.hProcess;
    return true;
}

unsigned long SessionAgentManager::find_session_for_user(const std::string& domain_user) const {
    const auto [expected_domain, expected_user] = split_domain_user(domain_user);

    WTS_SESSION_INFOW* sessions = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
        return kInvalidSession;
    }

    unsigned long found = kInvalidSession;
    for (DWORD i = 0; i < count; ++i) {
        const unsigned long session_id = sessions[i].SessionId;
        if (sessions[i].State != WTSActive && sessions[i].State != WTSConnected) {
            continue;
        }

        LPWSTR user_buffer = nullptr;
        DWORD bytes = 0;
        if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id, WTSUserName,
                                         &user_buffer, &bytes)) {
            continue;
        }
        const std::wstring user_name = user_buffer;
        WTSFreeMemory(user_buffer);

        LPWSTR domain_buffer = nullptr;
        std::wstring domain_name;
        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id, WTSDomainName,
                                        &domain_buffer, &bytes)) {
            domain_name = domain_buffer != nullptr ? domain_buffer : L"";
            WTSFreeMemory(domain_buffer);
        }

        if (!expected_domain.empty() &&
            _wcsicmp(domain_name.c_str(), expected_domain.c_str()) != 0) {
            continue;
        }
        if (_wcsicmp(user_name.c_str(), expected_user.c_str()) == 0) {
            found = session_id;
            break;
        }
    }

    WTSFreeMemory(sessions);
    return found;
}

}  // namespace useraudit
