#include "useraudit/crypto_aes_gcm.hpp"
#include "useraudit/log_crypto.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect_true(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

}  // namespace

int main() {
    using useraudit::AesGcmCipher;
    using useraudit::decrypt_log_line;
    using useraudit::encrypt_log_line;
    using useraudit::kEncryptedLogLinePrefix;

    std::array<std::uint8_t, AesGcmCipher::kKeySize> key{};
    for (std::size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<std::uint8_t>(i + 1);
    }

    const std::string plaintext =
        R"({"id":"11111111-1111-4111-8111-111111111111","ts":"2026-08-12T10:00:00.000Z","lvl":1,"cat":"session","act":"login","sev":"info","host":"NB-01","src":"eventlog"})";

    std::string encrypted;
    expect_true(encrypt_log_line(key.data(), key.size(), plaintext, encrypted), "encrypt line");
    expect_true(encrypted.rfind(kEncryptedLogLinePrefix, 0) == 0, "encrypted prefix");

    std::string decrypted;
    expect_true(decrypt_log_line(key.data(), key.size(), encrypted, decrypted), "decrypt line");
    expect_true(decrypted == plaintext, "roundtrip plaintext");

    std::string tampered = encrypted;
    tampered.back() = (tampered.back() == 'A') ? 'B' : 'A';
    expect_true(!decrypt_log_line(key.data(), key.size(), tampered, decrypted),
                "tampered ciphertext rejected");

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
