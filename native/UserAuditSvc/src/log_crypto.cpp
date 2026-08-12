#include "useraudit/log_crypto.hpp"

#include "useraudit/crypto_aes_gcm.hpp"
#include "useraudit/crypto_base64.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <vector>

namespace useraudit {

namespace {

bool generate_nonce(std::uint8_t* nonce, std::size_t size) {
    return nonce != nullptr && size == AesGcmCipher::kNonceSize &&
           BCRYPT_SUCCESS(BCryptGenRandom(nullptr, nonce, static_cast<ULONG>(size),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

}  // namespace

bool encrypt_log_line(const std::uint8_t* dek, std::size_t dek_size, const std::string& plaintext,
                      std::string& encrypted_line) {
    if (dek == nullptr || dek_size != AesGcmCipher::kKeySize ||
        plaintext.size() > kMaxPlainLogLineBytes) {
        return false;
    }

    std::uint8_t nonce[AesGcmCipher::kNonceSize]{};
    if (!generate_nonce(nonce, sizeof(nonce))) {
        return false;
    }

    AesGcmCipher cipher;
    std::vector<std::uint8_t> ciphertext;
    std::vector<std::uint8_t> tag;
    if (!cipher.encrypt(dek, dek_size, nonce, sizeof(nonce),
                        reinterpret_cast<const std::uint8_t*>(plaintext.data()), plaintext.size(),
                        ciphertext, tag)) {
        return false;
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(sizeof(nonce) + ciphertext.size() + tag.size());
    payload.insert(payload.end(), nonce, nonce + sizeof(nonce));
    payload.insert(payload.end(), ciphertext.begin(), ciphertext.end());
    payload.insert(payload.end(), tag.begin(), tag.end());

    const std::string encoded = base64_encode(payload.data(), payload.size());
    if (encoded.empty()) {
        return false;
    }

    encrypted_line = kEncryptedLogLinePrefix;
    encrypted_line += encoded;
    return true;
}

bool decrypt_log_line(const std::uint8_t* dek, std::size_t dek_size, const std::string& encrypted_line,
                      std::string& plaintext) {
    if (dek == nullptr || dek_size != AesGcmCipher::kKeySize ||
        encrypted_line.rfind(kEncryptedLogLinePrefix, 0) != 0) {
        return false;
    }

    const std::string encoded = encrypted_line.substr(sizeof(kEncryptedLogLinePrefix) - 1);
    std::vector<std::uint8_t> payload;
    if (!base64_decode(encoded, payload)) {
        return false;
    }

    if (payload.size() < AesGcmCipher::kNonceSize + AesGcmCipher::kTagSize) {
        return false;
    }

    const std::size_t ciphertext_size =
        payload.size() - AesGcmCipher::kNonceSize - AesGcmCipher::kTagSize;
    const std::uint8_t* nonce = payload.data();
    const std::uint8_t* ciphertext = payload.data() + AesGcmCipher::kNonceSize;
    const std::uint8_t* tag = payload.data() + AesGcmCipher::kNonceSize + ciphertext_size;

    AesGcmCipher cipher;
    std::vector<std::uint8_t> decrypted;
    if (!cipher.decrypt(dek, dek_size, nonce, AesGcmCipher::kNonceSize, ciphertext, ciphertext_size,
                         tag, AesGcmCipher::kTagSize, decrypted)) {
        return false;
    }

    plaintext.assign(reinterpret_cast<const char*>(decrypted.data()), decrypted.size());
    return true;
}

}  // namespace useraudit
