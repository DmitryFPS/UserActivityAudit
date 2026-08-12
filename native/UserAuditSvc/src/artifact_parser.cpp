#include "useraudit/artifact_parser.hpp"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace useraudit {

namespace {

std::string rot13(const std::string& value) {
    std::string out = value;
    for (char& ch : out) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>('a' + (ch - 'a' + 13) % 26);
        } else if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>('A' + (ch - 'A' + 13) % 26);
        }
    }
    return out;
}

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

void export_userassist_for_user(HKEY root, const std::wstring& sid, std::ofstream& output,
                                int& count) {
    const std::wstring base =
        sid + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist";
    HKEY userassist_key = nullptr;
    if (RegOpenKeyExW(root, base.c_str(), 0, KEY_READ, &userassist_key) != ERROR_SUCCESS) {
        return;
    }

    DWORD index = 0;
    wchar_t subkey_name[256];
    DWORD subkey_len = static_cast<DWORD>(sizeof(subkey_name) / sizeof(subkey_name[0]));

    while (RegEnumKeyExW(userassist_key, index, subkey_name, &subkey_len, nullptr, nullptr, nullptr,
                         nullptr) == ERROR_SUCCESS) {
        const std::wstring count_path = base + L"\\" + subkey_name + L"\\Count";
        HKEY count_key = nullptr;
        if (RegOpenKeyExW(root, count_path.c_str(), 0, KEY_READ, &count_key) == ERROR_SUCCESS) {
            DWORD value_index = 0;
            wchar_t value_name[512];
            DWORD value_name_len = static_cast<DWORD>(sizeof(value_name) / sizeof(value_name[0]));
            while (RegEnumValueW(count_key, value_index, value_name, &value_name_len, nullptr,
                                 nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                output << wide_to_utf8(sid) << '\t' << rot13(wide_to_utf8(value_name)) << '\n';
                ++count;
                value_name_len = static_cast<DWORD>(sizeof(value_name) / sizeof(value_name[0]));
                ++value_index;
            }
            RegCloseKey(count_key);
        }

        subkey_len = static_cast<DWORD>(sizeof(subkey_name) / sizeof(subkey_name[0]));
        ++index;
    }

    RegCloseKey(userassist_key);
}

}  // namespace

ArtifactCollectionResult collect_system_artifacts(const ForensicSettings& settings,
                                                    const std::filesystem::path& staging_dir,
                                                    std::string& error) {
    ArtifactCollectionResult result{};
    const auto artifact_dir = staging_dir / "artifacts";
    std::error_code ec;
    std::filesystem::create_directories(artifact_dir, ec);

    if (settings.artifact_prefetch) {
        const auto prefetch_dir = artifact_dir / "prefetch";
        std::filesystem::create_directories(prefetch_dir, ec);
        const auto manifest_path = prefetch_dir / "manifest.txt";
        std::ofstream manifest(manifest_path, std::ios::trunc);
        if (manifest.is_open()) {
            const std::filesystem::path source = L"C:\\Windows\\Prefetch";
            int copied = 0;
            const int limit = settings.max_prefetch_files > 0 ? settings.max_prefetch_files : 50;
            for (const auto& entry : std::filesystem::directory_iterator(
                     source, std::filesystem::directory_options::skip_permission_denied, ec)) {
                if (ec || !entry.is_regular_file()) {
                    continue;
                }
                if (entry.path().extension() != L".pf") {
                    continue;
                }
                manifest << entry.path().filename().string() << '\t' << entry.file_size(ec) << '\n';
                ++copied;
                if (copied >= limit) {
                    break;
                }
            }
            result.prefetch_files = copied;
            result.manifest_path = manifest_path;
        }
    }

    if (settings.artifact_userassist) {
        const auto userassist_path = artifact_dir / "userassist.tsv";
        std::ofstream output(userassist_path, std::ios::trunc);
        if (output.is_open()) {
            output << "sid\tprogram\n";
            HKEY users_key = nullptr;
            if (RegOpenKeyExW(HKEY_USERS, nullptr, 0, KEY_READ, &users_key) == ERROR_SUCCESS) {
                DWORD index = 0;
                wchar_t sid[256];
                DWORD sid_len = static_cast<DWORD>(sizeof(sid) / sizeof(sid[0]));
                while (RegEnumKeyExW(users_key, index, sid, &sid_len, nullptr, nullptr, nullptr,
                                     nullptr) == ERROR_SUCCESS) {
                    export_userassist_for_user(HKEY_USERS, sid, output, result.userassist_entries);
                    sid_len = static_cast<DWORD>(sizeof(sid) / sizeof(sid[0]));
                    ++index;
                }
                RegCloseKey(users_key);
            }
            result.manifest_path = userassist_path;
        }
    }

    error.clear();
    return result;
}

}  // namespace useraudit
