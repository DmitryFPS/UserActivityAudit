#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace useraudit {

class KeyManager {
public:
    static constexpr std::size_t kDekSize = 32;
    static constexpr std::size_t kHmacKeySize = 32;

    bool initialize(const std::filesystem::path& keys_directory);

    const std::uint8_t* dek() const { return dek_.data(); }
    const std::uint8_t* hmac_key() const { return hmac_key_.data(); }

    // Phase 3+: TPM seal replaces stub.
    bool tpm_seal_available() const { return false; }

private:
    bool load_or_create_keys(const std::filesystem::path& wrapped_key_path);
    bool generate_random(std::uint8_t* buffer, std::size_t size) const;
    bool wrap_and_save(const std::filesystem::path& wrapped_key_path);
    bool unwrap_from_file(const std::filesystem::path& wrapped_key_path);

    std::array<std::uint8_t, kDekSize> dek_{};
    std::array<std::uint8_t, kHmacKeySize> hmac_key_{};
    bool ready_ = false;
};

}  // namespace useraudit
