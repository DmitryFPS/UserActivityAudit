#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace useraudit {

class AesGcmCipher {
public:
    static constexpr std::size_t kKeySize = 32;
    static constexpr std::size_t kNonceSize = 12;
    static constexpr std::size_t kTagSize = 16;

    bool encrypt(const std::uint8_t* key, std::size_t key_size, const std::uint8_t* nonce,
                 std::size_t nonce_size, const std::uint8_t* plaintext, std::size_t plaintext_size,
                 std::vector<std::uint8_t>& ciphertext, std::vector<std::uint8_t>& tag);

    bool decrypt(const std::uint8_t* key, std::size_t key_size, const std::uint8_t* nonce,
                 std::size_t nonce_size, const std::uint8_t* ciphertext, std::size_t ciphertext_size,
                 const std::uint8_t* tag, std::size_t tag_size, std::vector<std::uint8_t>& plaintext);
};

}  // namespace useraudit
