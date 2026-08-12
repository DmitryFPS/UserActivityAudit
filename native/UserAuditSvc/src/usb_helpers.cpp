#include "useraudit/usb_helpers.hpp"

#include <algorithm>
#include <cctype>

namespace useraudit {

namespace {

std::string to_lower(std::string_view input) {
    std::string out(input);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}

std::string extract_token(std::string_view source, std::string_view prefix) {
    const std::string lower = to_lower(source);
    const std::string prefix_lower = to_lower(prefix);
    const size_t pos = lower.find(prefix_lower);
    if (pos == std::string::npos) {
        return {};
    }
    const size_t start = pos + prefix_lower.size();
    const size_t end_amp = lower.find('&', start);
    const size_t end_bs = lower.find('\\', start);
    size_t end = lower.size();
    if (end_amp != std::string::npos) {
        end = end_amp;
    }
    if (end_bs != std::string::npos && end_bs < end) {
        end = end_bs;
    }
    return lower.substr(start, end - start);
}

}  // namespace

bool parse_usb_vid_pid(std::string_view pnp_device_id, UsbVolumeIdentity& out) {
    if (pnp_device_id.empty()) {
        return false;
    }

    out.pnp_device_id = std::string(pnp_device_id);
    out.vid = extract_token(pnp_device_id, "vid_");
    out.pid = extract_token(pnp_device_id, "pid_");

    const auto last_backslash = pnp_device_id.find_last_of('\\');
    if (last_backslash != std::string_view::npos &&
        last_backslash + 1 < pnp_device_id.size()) {
        out.serial = std::string(pnp_device_id.substr(last_backslash + 1));
    }

    return !out.vid.empty() && !out.pid.empty();
}

}  // namespace useraudit
