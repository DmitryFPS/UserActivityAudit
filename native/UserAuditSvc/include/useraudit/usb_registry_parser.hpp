#pragma once

#include <filesystem>
#include <string>

namespace useraudit {

struct UsbRegistryResult {
    int key_lines = 0;
    std::filesystem::path output_path;
};

UsbRegistryResult export_usb_registry(const std::filesystem::path& staging_dir, std::string& error);

}  // namespace useraudit
