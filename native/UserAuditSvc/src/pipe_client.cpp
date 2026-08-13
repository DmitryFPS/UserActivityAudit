#include "useraudit/pipe_client.hpp"

#include "useraudit/event_serializer.hpp"
#include "useraudit/pipe_constants.hpp"

#include <vector>

namespace useraudit {

PipeClient::PipeClient() = default;

PipeClient::~PipeClient() {
    disconnect();
}

bool PipeClient::connect(unsigned retry_ms, unsigned max_attempts) {
    disconnect();

    for (unsigned attempt = 0; attempt < max_attempts; ++attempt) {
        pipe_ = CreateFileW(kEventPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                            nullptr);
        if (pipe_ != INVALID_HANDLE_VALUE) {
            return true;
        }
        Sleep(retry_ms);
    }

    return false;
}

void PipeClient::disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool PipeClient::try_write_payload(const std::string& json) {
    if (pipe_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    const auto length = static_cast<std::uint32_t>(json.size());

    DWORD written = 0;
    if (!WriteFile(pipe_, &length, sizeof(length), &written, nullptr) ||
        written != sizeof(length)) {
        return false;
    }

    if (length == 0) {
        return true;
    }

    written = 0;
    if (!WriteFile(pipe_, json.data(), length, &written, nullptr) || written != length) {
        return false;
    }

    return true;
}

bool PipeClient::write(const AuditEvent& event) {
    const std::string json = serialize_event_json(event);
    if (try_write_payload(json)) {
        return true;
    }

    disconnect();
    if (!connect(200, 15)) {
        return false;
    }

    return try_write_payload(json);
}

}  // namespace useraudit
