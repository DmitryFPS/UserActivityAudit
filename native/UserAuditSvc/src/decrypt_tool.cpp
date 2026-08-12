#include "useraudit/decrypt_tool.hpp"

#include "useraudit/hash_chain.hpp"
#include "useraudit/key_manager.hpp"
#include "useraudit/log_crypto.hpp"
#include "useraudit/paths.hpp"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace useraudit {

namespace {

bool has_flag(int argc, wchar_t** argv, const wchar_t* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::wstring(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

std::wstring get_arg_value(int argc, wchar_t** argv, const wchar_t* flag) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] != nullptr && std::wstring(argv[i]) == flag) {
            return argv[i + 1];
        }
    }
    return {};
}

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::string current_date_utc() {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    char buffer[16]{};
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    return buffer;
}

std::vector<std::filesystem::path> collect_log_files(const std::filesystem::path& log_directory,
                                                     const std::string& date_prefix) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(log_directory, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }

        const auto filename = entry.path().filename().string();
        if (filename.rfind(date_prefix, 0) != 0) {
            continue;
        }
        if (filename.size() < std::strlen(kEncryptedLogExtension) ||
            filename.compare(filename.size() - std::strlen(kEncryptedLogExtension),
                             std::strlen(kEncryptedLogExtension), kEncryptedLogExtension) != 0) {
            continue;
        }
        files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());
    return files;
}

void print_usage() {
    std::wcerr << L"Usage:\n"
               << L"  UserAudit.exe --decrypt [--date YYYY-MM-DD] [--verify]\n"
               << L"    Decrypt encrypted audit logs to stdout (JSONL).\n";
}

}  // namespace

int run_decrypt_mode(int argc, wchar_t** argv) {
    if (has_flag(argc, argv, L"--help") || has_flag(argc, argv, L"-h")) {
        print_usage();
        return 0;
    }

    const bool verify = has_flag(argc, argv, L"--verify");
    const std::wstring date_arg = get_arg_value(argc, argv, L"--date");
    const std::string date = date_arg.empty() ? current_date_utc() : wide_to_utf8(date_arg);
    if (date.size() != 10) {
        std::wcerr << L"Invalid --date format. Expected YYYY-MM-DD.\n";
        return 1;
    }

    KeyManager keys;
    if (!keys.initialize(resolve_keys_directory())) {
        std::wcerr << L"Failed to load encryption keys. Run as administrator or LocalSystem.\n";
        return 1;
    }

    HashChain chain;
    if (verify &&
        !chain.initialize(resolve_chain_state_path(), keys.hmac_key(), KeyManager::kHmacKeySize)) {
        std::wcerr << L"Failed to load hash chain state.\n";
        return 1;
    }

    const auto log_directory = resolve_log_directory();
    const auto files = collect_log_files(log_directory, date);
    if (files.empty()) {
        std::wcerr << L"No encrypted logs found for date " << date.c_str() << L" in "
                   << log_directory.wstring() << L"\n";
        return 1;
    }

    std::size_t lines = 0;
    std::size_t tamper_failures = 0;

    for (const auto& file : files) {
        std::ifstream input(file, std::ios::binary);
        if (!input.is_open()) {
            std::wcerr << L"Failed to open " << file.wstring() << L"\n";
            return 1;
        }

        std::string encrypted_line;
        while (std::getline(input, encrypted_line)) {
            if (encrypted_line.empty()) {
                continue;
            }

            std::string plaintext;
            if (!decrypt_log_line(keys.dek(), KeyManager::kDekSize, encrypted_line, plaintext)) {
                std::wcerr << L"Decrypt failed in " << file.wstring() << L"\n";
                return 1;
            }

            if (verify && !chain.verify_json_line(plaintext)) {
                ++tamper_failures;
                std::wcerr << L"TAMPER: hash chain verification failed\n";
            }

            std::cout << plaintext << '\n';
            ++lines;
        }
    }

    if (verify && tamper_failures > 0) {
        std::wcerr << L"Verification failed for " << tamper_failures << L" line(s).\n";
        return 2;
    }

    std::wcerr << L"Decrypted " << lines << L" event(s) from " << files.size() << L" file(s).\n";
    return 0;
}

}  // namespace useraudit
