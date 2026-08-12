#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace useraudit {

struct ZipEntry {
    std::filesystem::path source_path;
    std::string archive_name;
};

// Store-only ZIP (no compression) for forensic packs.
bool create_zip_store(const std::filesystem::path& zip_path, const std::vector<ZipEntry>& entries,
                      std::string& error);

}  // namespace useraudit
