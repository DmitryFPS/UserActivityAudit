#include "useraudit/pipe_server.hpp"

#include "useraudit/pipe_constants.hpp"

#include <windows.h>
#include <sddl.h>
#include <winbase.h>

#include <cstdint>
#include <vector>

namespace useraudit {

namespace {

#ifndef PIPE_ACCESS_INCOMING
#define PIPE_ACCESS_INCOMING PIPE_ACCESS_INBOUND
#endif

}  // namespace

PipeIngestServer::PipeIngestServer(EncryptedLogWriter& writer) : writer_(writer) {}

PipeIngestServer::~PipeIngestServer() {
    stop();
}

bool PipeIngestServer::start() {
    if (running_.load()) {
        return true;
    }

    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stop_event_ == nullptr) {
        return false;
    }

    running_.store(true);
    accept_thread_ = std::thread([this]() { accept_loop(); });
    return true;
}

void PipeIngestServer::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (stop_event_ != nullptr) {
        SetEvent(stop_event_);
    }

    HANDLE wake = CreateFileW(kEventPipeName, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (wake != INVALID_HANDLE_VALUE) {
        CloseHandle(wake);
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    if (stop_event_ != nullptr) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }
}

void PipeIngestServer::accept_loop() {
    static constexpr wchar_t kSddl[] = L"D:(A;;GA;;;SY)(A;;GA;;;AU)";
    PSECURITY_DESCRIPTOR sd = nullptr;
    SECURITY_ATTRIBUTES sa{};
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(kSddl, SDDL_REVISION_1, &sd,
                                                             nullptr)) {
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = sd;
        sa.bInheritHandle = FALSE;
    }

    while (running_.load()) {
        HANDLE pipe = CreateNamedPipeW(
            kEventPipeName, PIPE_ACCESS_INCOMING,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 65536, 65536,
            0, sa.lpSecurityDescriptor ? &sa : nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        const BOOL connected =
            ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!running_.load()) {
            CloseHandle(pipe);
            break;
        }

        if (!connected) {
            CloseHandle(pipe);
            continue;
        }

        std::thread([this, pipe]() { handle_client(pipe); }).detach();
    }
}

void PipeIngestServer::handle_client(HANDLE pipe) {
    while (running_.load()) {
        std::uint32_t length = 0;
        DWORD read = 0;
        if (!ReadFile(pipe, &length, sizeof(length), &read, nullptr) ||
            read != sizeof(length)) {
            break;
        }

        if (length == 0) {
            continue;
        }

        if (length > 1024 * 1024) {
            break;
        }

        std::vector<char> payload(length);
        if (!ReadFile(pipe, payload.data(), length, &read, nullptr) || read != length) {
            break;
        }

        writer_.write_raw_json_line(std::string(payload.data(), payload.size()));
    }

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

}  // namespace useraudit
