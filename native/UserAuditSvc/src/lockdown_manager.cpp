#include "useraudit/lockdown_manager.hpp"

#include "useraudit/driver_client.hpp"
#include "useraudit/time_utils.hpp"

namespace useraudit {

namespace {

LockdownManager* g_lockdown_manager = nullptr;

}  // namespace

void set_global_lockdown_manager(LockdownManager* manager) {
    g_lockdown_manager = manager;
}

LockdownManager* global_lockdown_manager() {
    return g_lockdown_manager;
}

LockdownManager::LockdownManager(EventSink& writer, std::string hostname)
    : writer_(writer), hostname_(std::move(hostname)) {}

void LockdownManager::emit_lockdown_event(const std::string& reason, bool enabled) {
    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 2;
    event.cat = "tamper";
    event.act = enabled ? "lockdown_on" : "lockdown_off";
    event.sev = "critical";
    event.host = hostname_;
    event.src = "lockdown";
    event.data["reason"] = reason;
    writer_.write(event);
}

void LockdownManager::activate(const std::string& reason) {
    if (active_.exchange(true)) {
        return;
    }
    global_driver_client().set_lockdown(true);
    emit_lockdown_event(reason, true);
}

void LockdownManager::deactivate() {
    if (!active_.exchange(false)) {
        return;
    }
    global_driver_client().set_lockdown(false);
    emit_lockdown_event("manual_clear", false);
}

}  // namespace useraudit
