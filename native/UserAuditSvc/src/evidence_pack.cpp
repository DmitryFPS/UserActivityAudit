#include "useraudit/evidence_pack.hpp"

#include "useraudit/artifact_parser.hpp"
#include "useraudit/browser_parser.hpp"
#include "useraudit/sqlite_loader.hpp"
#include "useraudit/time_utils.hpp"
#include "useraudit/usb_registry_parser.hpp"
#include "useraudit/zip_writer.hpp"

#include <fstream>
#include <sstream>

namespace useraudit {

namespace {

void remove_tree(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

void collect_files_recursive(const std::filesystem::path& root, const std::filesystem::path& base,
                             std::vector<ZipEntry>& entries) {
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const auto relative = std::filesystem::relative(entry.path(), base, ec);
        if (ec) {
            continue;
        }
        entries.push_back(ZipEntry{entry.path(), relative.generic_string()});
    }
}

}  // namespace

EvidencePackResult build_evidence_pack(const ForensicSettings& settings,
                                       const EvidencePackRequest& request,
                                       const std::filesystem::path& staging_root,
                                       const std::filesystem::path& packs_dir, std::string& error) {
    EvidencePackResult result{};

    const auto staging_dir = staging_root / (generate_event_id());
    std::error_code ec;
    std::filesystem::create_directories(staging_dir, ec);

    SqliteLoader sqlite;
    const auto browser = collect_browser_artifacts(settings, staging_dir, sqlite, error);
    result.browser_history_rows = browser.history_rows;
    result.browser_download_rows = browser.download_rows;

    const auto artifacts = collect_system_artifacts(settings, staging_dir, error);
    result.prefetch_files = artifacts.prefetch_files;

    if (settings.artifact_usb_registry) {
        const auto usb = export_usb_registry(staging_dir, error);
        result.usb_registry_lines = usb.key_lines;
    }

    const auto trigger_path = staging_dir / "trigger.json";
    {
        std::ofstream trigger_file(trigger_path, std::ios::trunc);
        trigger_file << "{\n"
                     << "  \"trigger\": \"" << json_escape(request.trigger) << "\",\n"
                     << "  \"corr\": \"" << json_escape(request.corr) << "\",\n"
                     << "  \"exported_at_utc\": \"" << utc_now_iso8601() << "\"\n"
                     << "}\n";
    }

    if (!request.usb_context.empty()) {
        const auto usb_context_path = staging_dir / "usb_context.json";
        std::ofstream usb_file(usb_context_path, std::ios::trunc);
        usb_file << "{\n";
        bool first = true;
        for (const auto& [key, value] : request.usb_context) {
            if (!first) {
                usb_file << ",\n";
            }
            usb_file << "  \"" << json_escape(key) << "\": \"" << json_escape(value) << '"';
            first = false;
        }
        usb_file << "\n}\n";
    }

    const auto manifest_path = staging_dir / "manifest.json";
    {
        std::ofstream manifest(manifest_path, std::ios::trunc);
        manifest << "{\n"
                 << "  \"trigger\": \"" << json_escape(request.trigger) << "\",\n"
                 << "  \"corr\": \"" << json_escape(request.corr) << "\",\n"
                 << "  \"exported_at_utc\": \"" << utc_now_iso8601() << "\",\n"
                 << "  \"browser_history_rows\": " << result.browser_history_rows << ",\n"
                 << "  \"browser_download_rows\": " << result.browser_download_rows << ",\n"
                 << "  \"prefetch_files\": " << result.prefetch_files << ",\n"
                 << "  \"usb_registry_lines\": " << result.usb_registry_lines << "\n"
                 << "}\n";
    }

    std::vector<ZipEntry> entries;
    collect_files_recursive(staging_dir, staging_dir, entries);
    result.artifact_files = static_cast<int>(entries.size());

    std::filesystem::create_directories(packs_dir, ec);
    const std::string pack_name =
        "pack-" + (request.corr.empty() ? generate_event_id() : request.corr) + "-" +
        generate_event_id().substr(0, 8) + ".zip";
    result.zip_path = packs_dir / pack_name;

    if (!create_zip_store(result.zip_path, entries, error)) {
        remove_tree(staging_dir);
        return result;
    }

    remove_tree(staging_dir);
    error.clear();
    return result;
}

}  // namespace useraudit
