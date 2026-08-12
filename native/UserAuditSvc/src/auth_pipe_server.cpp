#include "useraudit/auth_pipe_server.hpp"

#include "useraudit/auth_protocol.hpp"
#include "useraudit/paths.hpp"

#include <windows.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace useraudit {

namespace {

constexpr wchar_t kAuthPipeName[] = L"\\\\.\\pipe\\UserAudit\\Auth";
constexpr char kCmdGetChallenge[] = "GET_CHALLENGE";
constexpr char kCmdSubmitToken[] = "SUBMIT_TOKEN";

std::string get_hostname_utf8_local() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(buffer, &size)) {
        return "unknown";
    }
    char out[256]{};
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, out, sizeof(out), nullptr, nullptr);
    return out;
}

bool write_all(HANDLE pipe, const void* data, DWORD size) {
    DWORD written = 0;
    return WriteFile(pipe, data, size, &written, nullptr) && written == size;
}

bool read_all(HANDLE pipe, void* data, DWORD size) {
    DWORD read = 0;
    return ReadFile(pipe, data, size, &read, nullptr) && read == size;
}

}  // namespace

AuthPipeServer::AuthPipeServer(AuthGuard& guard) : guard_(guard) {}

AuthPipeServer::~AuthPipeServer() {
    stop();
}

bool AuthPipeServer::handle_client(void* pipe_handle) {
    auto* pipe = static_cast<HANDLE>(pipe_handle);
    char command[32]{};
    if (!read_all(pipe, command, sizeof(command))) {
        return false;
    }

    if (std::string(command).find(kCmdGetChallenge) == 0) {
        AuthAction action = AuthAction::StopService;
        if (std::string(command).find("UNINSTALL") != std::string::npos) {
            action = AuthAction::Uninstall;
        }
        const AuthChallenge challenge = guard_.create_challenge(action, get_hostname_utf8_local());
        const auto payload = serialize_challenge_for_signing(challenge);
        const DWORD payload_size = static_cast<DWORD>(payload.size());
        if (!write_all(pipe, &payload_size, sizeof(payload_size))) {
            return false;
        }
        return write_all(pipe, payload.data(), payload_size);
    }

    if (std::string(command).find(kCmdSubmitToken) == 0) {
        DWORD token_size = 0;
        if (!read_all(pipe, &token_size, sizeof(token_size)) || token_size > 4096) {
            return false;
        }
        std::vector<std::uint8_t> bytes(token_size);
        if (!read_all(pipe, bytes.data(), token_size)) {
            return false;
        }
        AuthToken token{};
        const bool ok = parse_auth_token_bytes(bytes, token) && guard_.submit_token(token);
        const std::uint8_t result = ok ? 1 : 0;
        const DWORD result_size = 1;
        if (!write_all(pipe, &result_size, sizeof(result_size))) {
            return false;
        }
        return write_all(pipe, &result, sizeof(result));
    }

    return false;
}

void AuthPipeServer::server_thread_main() {
    while (running_.load() && (stop_flag_ == nullptr || !*stop_flag_)) {
        HANDLE pipe = CreateNamedPipeW(
            kAuthPipeName, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 4096, 4096,
            0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        const BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            handle_client(pipe);
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
    running_.store(false);
}

bool AuthPipeServer::start() {
    if (running_.load()) {
        return true;
    }
    running_.store(true);
    server_thread_ = std::thread([this]() { server_thread_main(); });
    return true;
}

void AuthPipeServer::stop() {
    running_.store(false);
    HANDLE wake = CreateFileW(kAuthPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              0, nullptr);
    if (wake != INVALID_HANDLE_VALUE) {
        CloseHandle(wake);
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

}  // namespace useraudit
