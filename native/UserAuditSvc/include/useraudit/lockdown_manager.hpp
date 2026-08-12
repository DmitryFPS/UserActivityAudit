#pragma once

#include "useraudit/event_sink.hpp"

#include <atomic>
#include <string>

namespace useraudit {

class LockdownManager {
public:
    LockdownManager(EventSink& writer, std::string hostname);

    [[nodiscard]] bool active() const { return active_.load(); }
    void activate(const std::string& reason);
    void deactivate();

private:
    void emit_lockdown_event(const std::string& reason, bool enabled);

    EventSink& writer_;
    std::string hostname_;
    std::atomic<bool> active_{false};
};

LockdownManager* global_lockdown_manager();
void set_global_lockdown_manager(LockdownManager* manager);

}  // namespace useraudit
