#include "useraudit/event_loop.hpp"

#include <windows.h>

#include <chrono>
#include <thread>

namespace useraudit {

void run_event_loop(volatile bool& stop_requested) {
    // Phase 1: SessionCollector, ProcessCollector, ForegroundCollector, UsbCollector
    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

}  // namespace useraudit
