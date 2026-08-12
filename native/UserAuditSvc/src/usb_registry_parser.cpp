#include "useraudit/usb_registry_parser.hpp"

#include <windows.h>

#include <fstream>

namespace useraudit {

namespace {

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

void dump_registry_tree(HKEY root, const std::wstring& subkey, int depth, std::ofstream& output,
                        int& lines) {
    if (depth > 6) {
        return;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;
    }

    output << wide_to_utf8(subkey) << '\n';
    ++lines;

    DWORD value_index = 0;
    wchar_t value_name[512];
    BYTE data[1024];
    DWORD value_name_len = static_cast<DWORD>(sizeof(value_name) / sizeof(value_name[0]));
    DWORD data_len = static_cast<DWORD>(sizeof(data));
    DWORD type = 0;

    while (RegEnumValueW(key, value_index, value_name, &value_name_len, nullptr, &type, data,
                         &data_len) == ERROR_SUCCESS) {
        output << "  " << wide_to_utf8(value_name) << "=" << type << '\n';
        ++lines;
        value_name_len = static_cast<DWORD>(sizeof(value_name) / sizeof(value_name[0]));
        data_len = static_cast<DWORD>(sizeof(data));
        ++value_index;
    }

    DWORD sub_index = 0;
    wchar_t child_name[256];
    DWORD child_len = static_cast<DWORD>(sizeof(child_name) / sizeof(child_name[0]));
    while (RegEnumKeyExW(key, sub_index, child_name, &child_len, nullptr, nullptr, nullptr,
                         nullptr) == ERROR_SUCCESS) {
        dump_registry_tree(root, subkey + L"\\" + child_name, depth + 1, output, lines);
        child_len = static_cast<DWORD>(sizeof(child_name) / sizeof(child_name[0]));
        ++sub_index;
    }

    RegCloseKey(key);
}

}  // namespace

UsbRegistryResult export_usb_registry(const std::filesystem::path& staging_dir, std::string& error) {
    UsbRegistryResult result{};
    const auto registry_dir = staging_dir / "registry";
    std::error_code ec;
    std::filesystem::create_directories(registry_dir, ec);

    result.output_path = registry_dir / "usbstor.txt";
    std::ofstream output(result.output_path, std::ios::trunc);
    if (!output.is_open()) {
        error = "cannot write usb registry export";
        return result;
    }

    output << "# USBSTOR device history (UsbForensicAudit-compatible export)\n";
    dump_registry_tree(HKEY_LOCAL_MACHINE,
                       L"SYSTEM\\CurrentControlSet\\Enum\\USBSTOR", 0, output, result.key_lines);

    error.clear();
    return result;
}

}  // namespace useraudit
