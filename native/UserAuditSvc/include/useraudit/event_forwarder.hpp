#pragma once

#include "useraudit/audit_event.hpp"
#include "useraudit/upload_client.hpp"

namespace useraudit {

// POST tamper/critical events to Ingest /api/v1/ingest/events (WinHTTP, fire-and-forget).
class EventForwarder {
public:
    explicit EventForwarder(UploadSettings settings);

    EventForwarder(const EventForwarder&) = delete;
    EventForwarder& operator=(const EventForwarder&) = delete;

    [[nodiscard]] bool enabled() const { return settings_.mode == UploadMode::Http; }

    void forward(const AuditEvent& event);

private:
    UploadSettings settings_;
};

}  // namespace useraudit
