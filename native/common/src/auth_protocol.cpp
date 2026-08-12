#include "useraudit/auth_protocol.hpp"

extern "C" {
#include "ed25519.h"
}

#include <array>
#include <cstring>

namespace useraudit {

namespace {

void append_le64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

std::uint64_t read_le64(const std::uint8_t* data) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(data[i]) << (8 * i);
    }
    return value;
}

}  // namespace

std::vector<std::uint8_t> serialize_challenge_for_signing(const AuthChallenge& challenge) {
    std::vector<std::uint8_t> payload;
    payload.reserve(8 + kAuthChallengeNonceSize + 1 + kAuthHostFieldSize);
    append_le64(payload, challenge.expiry_unix);
    payload.insert(payload.end(), challenge.nonce.begin(), challenge.nonce.end());
    payload.push_back(static_cast<std::uint8_t>(challenge.action));
    payload.insert(payload.end(), challenge.host, challenge.host + kAuthHostFieldSize);
    return payload;
}

std::vector<std::uint8_t> serialize_auth_token(const AuthToken& token) {
    std::vector<std::uint8_t> bytes;
    const auto payload = serialize_challenge_for_signing(token.challenge);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    bytes.insert(bytes.end(), token.signature.begin(), token.signature.end());
    return bytes;
}

bool parse_auth_token_bytes(const std::vector<std::uint8_t>& bytes, AuthToken& out) {
    const std::size_t expected = 8 + kAuthChallengeNonceSize + 1 + kAuthHostFieldSize + kEd25519SignatureSize;
    if (bytes.size() < expected) {
        return false;
    }

    std::size_t offset = 0;
    out.challenge.expiry_unix = read_le64(bytes.data() + offset);
    offset += 8;
    std::memcpy(out.challenge.nonce.data(), bytes.data() + offset, kAuthChallengeNonceSize);
    offset += kAuthChallengeNonceSize;
    out.challenge.action = static_cast<AuthAction>(bytes[offset++]);
    std::memcpy(out.challenge.host, bytes.data() + offset, kAuthHostFieldSize);
    offset += kAuthHostFieldSize;
    std::memcpy(out.signature.data(), bytes.data() + offset, kEd25519SignatureSize);
    return true;
}

bool verify_auth_token(const AuthToken& token,
                       const std::array<std::uint8_t, kEd25519PublicKeySize>& org_public_key) {
    const auto payload = serialize_challenge_for_signing(token.challenge);
    return ed25519_verify(token.signature.data(), payload.data(), payload.size(),
                          org_public_key.data()) == 1;
}

void sign_auth_token(AuthToken& token,
                     const std::array<std::uint8_t, kEd25519PrivateKeySize>& org_private_key) {
    const auto payload = serialize_challenge_for_signing(token.challenge);
    std::array<std::uint8_t, kEd25519PublicKeySize> public_key{};
    std::memcpy(public_key.data(), org_private_key.data() + 32, kEd25519PublicKeySize);
    ed25519_sign(token.signature.data(), payload.data(), payload.size(), public_key.data(),
                 org_private_key.data());
}

}  // namespace useraudit
