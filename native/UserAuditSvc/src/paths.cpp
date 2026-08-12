#include "useraudit/paths.hpp"

#include <windows.h>

#include <ShlObj.h>

#include <array>

namespace useraudit {

namespace {

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

}  // namespace

std::filesystem::path resolve_log_directory() {
    PWSTR program_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &program_data)) &&
        program_data != nullptr) {
        std::filesystem::path path = program_data;
        CoTaskMemFree(program_data);
        return path / L"UserAudit" / L"logs";
    }

    return std::filesystem::path(L"C:\\ProgramData\\UserAudit\\logs");
}

std::filesystem::path resolve_keys_directory() {
    PWSTR program_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &program_data)) &&
        program_data != nullptr) {
        std::filesystem::path path = program_data;
        CoTaskMemFree(program_data);
        return path / L"UserAudit" / L"keys";
    }

    return std::filesystem::path(L"C:\\ProgramData\\UserAudit\\keys");
}

std::filesystem::path resolve_config_path() {
    PWSTR program_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &program_data)) &&
        program_data != nullptr) {
        std::filesystem::path path = program_data;
        CoTaskMemFree(program_data);
        return path / L"UserAudit" / L"config.json";
    }

    return std::filesystem::path(L"C:\\ProgramData\\UserAudit\\config.json");
}

std::filesystem::path resolve_chain_state_path() {
    return resolve_keys_directory() / L"chain.state";
}

std::string get_hostname_utf8() {
    std::array<wchar_t, MAX_COMPUTERNAME_LENGTH + 1> buffer{};
    DWORD size = static_cast<DWORD>(buffer.size());
    if (!GetComputerNameW(buffer.data(), &size)) {
        return "unknown";
    }
    return wide_to_utf8(std::wstring_view(buffer.data(), size));
}

}  // namespace useraudit
