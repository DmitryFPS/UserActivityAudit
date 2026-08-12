#include "useraudit/key_manager.hpp"

#include <windows.h>

#include <bcrypt.h>

#include <filesystem>
#include <vector>

namespace useraudit {

namespace {

constexpr wchar_t kWrappedKeyFile[] = L"master.key.dpapi";

}  // namespace

bool KeyManager::generate_random(std::uint8_t* buffer, std::size_t size) const {
    return buffer != nullptr && size > 0 &&
           BCRYPT_SUCCESS(BCryptGenRandom(nullptr, buffer, static_cast<ULONG>(size),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

bool KeyManager::wrap_and_save(const std::filesystem::path& wrapped_key_path) {
    std::vector<std::uint8_t> material(kDekSize + kHmacKeySize);
    if (!generate_random(material.data(), material.size())) {
        return false;
    }

    std::copy_n(material.begin(), kDekSize, dek_.begin());
    std::copy_n(material.begin() + kDekSize, kHmacKeySize, hmac_key_.begin());

    DATA_BLOB input{};
    input.pbData = material.data();
    input.cbData = static_cast<DWORD>(material.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"UserAudit master key", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_LOCAL_MACHINE, &output)) {
        SecureZeroMemory(material.data(), material.size());
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(wrapped_key_path.parent_path(), ec);

    HANDLE file = CreateFileW(wrapped_key_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        LocalFree(output.pbData);
        SecureZeroMemory(material.data(), material.size());
        return false;
    }

    DWORD written = 0;
    const BOOL ok =
        WriteFile(file, output.pbData, output.cbData, &written, nullptr) && written == output.cbData;
    CloseHandle(file);
    LocalFree(output.pbData);
    SecureZeroMemory(material.data(), material.size());

    return ok;
}

bool KeyManager::unwrap_from_file(const std::filesystem::path& wrapped_key_path) {
    HANDLE file = CreateFileW(wrapped_key_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0) {
        CloseHandle(file);
        return false;
    }

    std::vector<std::uint8_t> encrypted(size);
    DWORD read = 0;
    const BOOL read_ok = ReadFile(file, encrypted.data(), size, &read, nullptr);
    CloseHandle(file);
    if (!read_ok || read != size) {
        return false;
    }

    DATA_BLOB input{};
    input.pbData = encrypted.data();
    input.cbData = size;

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        return false;
    }

    if (output.cbData != kDekSize + kHmacKeySize) {
        LocalFree(output.pbData);
        return false;
    }

    std::copy_n(output.pbData, kDekSize, dek_.begin());
    std::copy_n(output.pbData + kDekSize, kHmacKeySize, hmac_key_.begin());
    LocalFree(output.pbData);
    return true;
}

bool KeyManager::load_or_create_keys(const std::filesystem::path& wrapped_key_path) {
    if (std::filesystem::exists(wrapped_key_path) && unwrap_from_file(wrapped_key_path)) {
        return true;
    }
    return wrap_and_save(wrapped_key_path);
}

bool KeyManager::initialize(const std::filesystem::path& keys_directory) {
    if (ready_) {
        return true;
    }

    const auto wrapped_key_path = keys_directory / kWrappedKeyFile;
    if (!load_or_create_keys(wrapped_key_path)) {
        return false;
    }

    ready_ = true;
    return true;
}

}  // namespace useraudit
