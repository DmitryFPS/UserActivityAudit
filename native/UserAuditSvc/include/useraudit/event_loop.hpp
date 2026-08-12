#pragma once

namespace useraudit {

// Main agent loop placeholder. Collectors attach here in Phase 1+.
void run_event_loop(volatile bool& stop_requested);

}  // namespace useraudit
