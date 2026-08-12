#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace useraudit {

// Связывает букву съёмного диска (E:) с correlation ID для цепочки USB → file.
class CorrelationTracker {
public:
    std::string on_usb_insert(const std::string& drive);
    void on_usb_remove(const std::string& drive);
    std::string correlation_for_drive(const std::string& drive) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> drive_to_corr_;
};

}  // namespace useraudit
