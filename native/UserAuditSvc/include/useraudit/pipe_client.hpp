#pragma once

#include "useraudit/event_sink.hpp"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace useraudit {

class PipeClient final : public EventSink {
public:
    PipeClient();
    ~PipeClient() override;

    PipeClient(const PipeClient&) = delete;
    PipeClient& operator=(const PipeClient&) = delete;

    bool connect(unsigned retry_ms = 500, unsigned max_attempts = 120);
    void disconnect();

    bool write(const AuditEvent& event) override;

private:
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
};

}  // namespace useraudit
