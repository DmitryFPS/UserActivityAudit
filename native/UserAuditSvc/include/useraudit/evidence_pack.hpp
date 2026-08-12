#pragma once

#include "useraudit/audit_event.hpp"
#include "useraudit/config_manager.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace useraudit {

struct EvidencePackResult {
    std::filesystem::path zip_path;
    int artifact_files = 0;
    int browser_history_rows = 0;
    int browser_download_rows = 0;
    int prefetch_files = 0;
    int usb_registry_lines = 0;
};

struct EvidencePackRequest {
    std::string trigger = "manual";
    std::string corr;
    std::map<std::string, std::string> usb_context;
};

EvidencePackResult build_evidence_pack(const ForensicSettings& settings,
                                       const EvidencePackRequest& request,
                                       const std::filesystem::path& staging_root,
                                       const std::filesystem::path& packs_dir, std::string& error);

}  // namespace useraudit
