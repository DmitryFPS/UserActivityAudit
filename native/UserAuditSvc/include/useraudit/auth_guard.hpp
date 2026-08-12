#pragma once

#include "useraudit/auth_protocol.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace useraudit {

class AuthGuard {
public:
    AuthGuard();

    bool load_org_public_key();
    [[nodiscard]] bool org_key_required() const { return org_key_loaded_; }

    AuthChallenge create_challenge(AuthAction action, const std::string& host);
    bool submit_token(const AuthToken& token);
    [[nodiscard]] bool is_authorized(AuthAction action) const;

    void clear_authorization();

private:
    [[nodiscard]] std::uint64_t now_unix() const;

    mutable std::mutex mutex_;
    std::array<std::uint8_t, kEd25519PublicKeySize> org_public_key_{};
    bool org_key_loaded_ = false;
    std::uint64_t stop_authorized_until_ = 0;
    std::uint64_t uninstall_authorized_until_ = 0;
};

std::filesystem::path resolve_org_public_key_path();
std::filesystem::path resolve_org_private_key_path(const std::filesystem::path& usb_root);

bool load_org_public_key_file(const std::filesystem::path& path,
                              std::array<std::uint8_t, kEd25519PublicKeySize>& out);
bool load_org_private_key_file(const std::filesystem::path& path,
                               std::array<std::uint8_t, kEd25519PrivateKeySize>& out);
bool save_org_keypair(const std::filesystem::path& directory,
                      const std::array<std::uint8_t, kEd25519PublicKeySize>& public_key,
                      const std::array<std::uint8_t, kEd25519PrivateKeySize>& private_key);

AuthGuard& global_auth_guard();
void set_global_auth_guard(AuthGuard* guard);

}  // namespace useraudit
