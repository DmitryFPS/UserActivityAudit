#include "useraudit/crypto_aes_gcm.hpp"

#include <bcrypt.h>

namespace useraudit {

namespace {

bool open_aes_gcm(BCRYPT_ALG_HANDLE& algorithm, BCRYPT_KEY_HANDLE& key_handle, const std::uint8_t* key,
                  std::size_t key_size) {
    algorithm = nullptr;
    key_handle = nullptr;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0))) {
        return false;
    }

    if (!BCRYPT_SUCCESS(BCryptSetProperty(
            algorithm, BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
            sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        algorithm = nullptr;
        return false;
    }

    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(algorithm, &key_handle, nullptr, 0,
                                                 const_cast<PUCHAR>(key), static_cast<ULONG>(key_size),
                                                 0))) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        algorithm = nullptr;
        return false;
    }

    return true;
}

void close_aes_gcm(BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE key_handle) {
    if (key_handle != nullptr) {
        BCryptDestroyKey(key_handle);
    }
    if (algorithm != nullptr) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }
}

}  // namespace

bool AesGcmCipher::encrypt(const std::uint8_t* key, std::size_t key_size, const std::uint8_t* nonce,
                           std::size_t nonce_size, const std::uint8_t* plaintext,
                           std::size_t plaintext_size, std::vector<std::uint8_t>& ciphertext,
                           std::vector<std::uint8_t>& tag) {
    if (key == nullptr || key_size != kKeySize || nonce == nullptr || nonce_size != kNonceSize) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key_handle = nullptr;
    if (!open_aes_gcm(algorithm, key_handle, key, key_size)) {
        return false;
    }

    tag.assign(kTagSize, 0);
    ciphertext.assign(plaintext_size, 0);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = const_cast<PUCHAR>(nonce);
    auth_info.cbNonce = static_cast<ULONG>(nonce_size);
    auth_info.pbTag = tag.data();
    auth_info.cbTag = static_cast<ULONG>(kTagSize);

    ULONG ciphertext_size = 0;
    const NTSTATUS status = BCryptEncrypt(
        key_handle, const_cast<PUCHAR>(plaintext), static_cast<ULONG>(plaintext_size), &auth_info,
        nullptr, 0, ciphertext.data(), static_cast<ULONG>(ciphertext.size()), &ciphertext_size, 0);

    close_aes_gcm(algorithm, key_handle);

    if (!BCRYPT_SUCCESS(status)) {
        ciphertext.clear();
        tag.clear();
        return false;
    }

    ciphertext.resize(ciphertext_size);
    return true;
}

bool AesGcmCipher::decrypt(const std::uint8_t* key, std::size_t key_size, const std::uint8_t* nonce,
                           std::size_t nonce_size, const std::uint8_t* ciphertext,
                           std::size_t ciphertext_size, const std::uint8_t* tag, std::size_t tag_size,
                           std::vector<std::uint8_t>& plaintext) {
    if (key == nullptr || key_size != kKeySize || nonce == nullptr || nonce_size != kNonceSize ||
        tag == nullptr || tag_size != kTagSize) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key_handle = nullptr;
    if (!open_aes_gcm(algorithm, key_handle, key, key_size)) {
        return false;
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = const_cast<PUCHAR>(nonce);
    auth_info.cbNonce = static_cast<ULONG>(nonce_size);
    auth_info.pbTag = const_cast<PUCHAR>(tag);
    auth_info.cbTag = static_cast<ULONG>(tag_size);

    plaintext.assign(ciphertext_size, 0);
    ULONG plaintext_size = 0;
    const NTSTATUS status =
        BCryptDecrypt(key_handle, const_cast<PUCHAR>(ciphertext), static_cast<ULONG>(ciphertext_size),
                      &auth_info, nullptr, 0, plaintext.data(), static_cast<ULONG>(plaintext.size()),
                      &plaintext_size, 0);

    close_aes_gcm(algorithm, key_handle);

    if (!BCRYPT_SUCCESS(status)) {
        plaintext.clear();
        return false;
    }

    plaintext.resize(plaintext_size);
    return true;
}

}  // namespace useraudit
