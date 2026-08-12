#include "useraudit/auth_protocol.hpp"

extern "C" {
#include "ed25519.h"
}

#include <array>
#include <cstdlib>
#include <iostream>

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
    std::array<std::uint8_t, useraudit::kEd25519PublicKeySize> public_key{};
    std::array<std::uint8_t, useraudit::kEd25519PrivateKeySize> private_key{};
    std::array<std::uint8_t, 32> seed{};
    for (std::size_t i = 0; i < seed.size(); ++i) {
        seed[i] = static_cast<std::uint8_t>(i + 1);
    }

    ed25519_create_keypair(public_key.data(), private_key.data(), seed.data());
    std::memcpy(private_key.data() + 32, public_key.data(), 32);

    useraudit::AuthChallenge challenge{};
    challenge.expiry_unix = 1'700'000'000ULL;
    challenge.action = useraudit::AuthAction::StopService;
    challenge.nonce.fill(0xAB);
    std::memcpy(challenge.host, "TESTHOST", 8);

    useraudit::AuthToken token{};
    token.challenge = challenge;
    useraudit::sign_auth_token(token, private_key);
    expect_true(useraudit::verify_auth_token(token, public_key), "verify valid token");

    token.signature[0] ^= 0xFF;
    expect_true(!useraudit::verify_auth_token(token, public_key), "reject tampered signature");

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
