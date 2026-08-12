#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace useraudit {

inline constexpr std::size_t kEd25519PublicKeySize = 32;
inline constexpr std::size_t kEd25519PrivateKeySize = 64;
inline constexpr std::size_t kEd25519SignatureSize = 64;
inline constexpr std::size_t kAuthChallengeNonceSize = 32;
inline constexpr std::size_t kAuthHostFieldSize = 64;

enum class AuthAction : std::uint8_t { StopService = 1, Uninstall = 2 };

struct AuthChallenge {
    std::uint64_t expiry_unix = 0;
    std::array<std::uint8_t, kAuthChallengeNonceSize> nonce{};
    AuthAction action = AuthAction::StopService;
    char host[kAuthHostFieldSize]{};
};

struct AuthToken {
    AuthChallenge challenge{};
    std::array<std::uint8_t, kEd25519SignatureSize> signature{};
};

std::vector<std::uint8_t> serialize_challenge_for_signing(const AuthChallenge& challenge);
bool parse_auth_token_bytes(const std::vector<std::uint8_t>& bytes, AuthToken& out);
std::vector<std::uint8_t> serialize_auth_token(const AuthToken& token);
bool verify_auth_token(const AuthToken& token, const std::array<std::uint8_t, kEd25519PublicKeySize>& org_public_key);
void sign_auth_token(AuthToken& token, const std::array<std::uint8_t, kEd25519PrivateKeySize>& org_private_key);

}  // namespace useraudit
