#pragma once

#include "useraudit/config_manager.hpp"

#include <filesystem>
#include <string>

namespace useraudit {

struct ArtifactCollectionResult {
    int prefetch_files = 0;
    int userassist_entries = 0;
    std::filesystem::path manifest_path;
};

ArtifactCollectionResult collect_system_artifacts(const ForensicSettings& settings,
                                                    const std::filesystem::path& staging_dir,
                                                    std::string& error);

}  // namespace useraudit
