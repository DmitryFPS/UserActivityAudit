#pragma once

#include <string>
#include <string_view>

namespace useraudit {

struct UsbVolumeIdentity {
    std::string vid;
    std::string pid;
    std::string serial;
    std::string pnp_device_id;
    std::string interface_type;
};

// Parse USB\VID_0411&PID_0397\... style PNP device IDs.
bool parse_usb_vid_pid(std::string_view pnp_device_id, UsbVolumeIdentity& out);

}  // namespace useraudit
