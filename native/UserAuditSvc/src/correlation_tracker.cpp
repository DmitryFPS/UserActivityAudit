#include "useraudit/correlation_tracker.hpp"

#include "useraudit/time_utils.hpp"

#include <cctype>

namespace useraudit {

namespace {

std::string normalize_drive_key(std::string drive) {
    if (drive.size() >= 2 && drive[1] == ':') {
        drive[0] = static_cast<char>(toupper(static_cast<unsigned char>(drive[0])));
        return drive.substr(0, 2);
    }
    return drive;
}

}  // namespace

std::string CorrelationTracker::on_usb_insert(const std::string& drive) {
    const std::string key = normalize_drive_key(drive);
    const std::string corr = generate_event_id();

    std::lock_guard lock(mutex_);
    drive_to_corr_[key] = corr;
    return corr;
}

void CorrelationTracker::on_usb_remove(const std::string& drive) {
    const std::string key = normalize_drive_key(drive);
    std::lock_guard lock(mutex_);
    drive_to_corr_.erase(key);
}

std::string CorrelationTracker::correlation_for_drive(const std::string& drive) const {
    const std::string key = normalize_drive_key(drive);
    std::lock_guard lock(mutex_);
    const auto it = drive_to_corr_.find(key);
    return it != drive_to_corr_.end() ? it->second : std::string{};
}

}  // namespace useraudit
