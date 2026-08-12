#pragma once

#include "useraudit/audit_event.hpp"

namespace useraudit {

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual bool write(const AuditEvent& event) = 0;
};

}  // namespace useraudit
