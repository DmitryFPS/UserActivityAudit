#include "useraudit/auth_guard.hpp"

#include "useraudit/paths.hpp"

#include <windows.h>
#include <bcrypt.h>

extern "C" {
#include "ed25519.h"
}

#include <chrono>
#include <fstream>

namespace useraudit {

namespace {

AuthGuard* g_auth_guard = nullptr;

}  // namespace

AuthGuard& global_auth_guard() {
    static AuthGuard instance;
    return instance;
}

void set_global_auth_guard(AuthGuard* guard) {
    g_auth_guard = guard;
}

std::filesystem::path resolve_org_public_key_path() {
    return resolve_keys_directory() / L"org.pub";
}

std::filesystem::path resolve_org_private_key_path(const std::filesystem::path& usb_root) {
    return usb_root / L"org.key";
}

bool load_org_public_key_file(const std::filesystem::path& path,
                              std::array<std::uint8_t, kEd25519PublicKeySize>& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    input.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return input.gcount() == static_cast<std::streamsize>(out.size());
}

bool load_org_private_key_file(const std::filesystem::path& path,
                               std::array<std::uint8_t, kEd25519PrivateKeySize>& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    input.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return input.gcount() == static_cast<std::streamsize>(out.size());
}

bool save_org_keypair(const std::filesystem::path& directory,
                      const std::array<std::uint8_t, kEd25519PublicKeySize>& public_key,
                      const std::array<std::uint8_t, kEd25519PrivateKeySize>& private_key) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);

    {
        std::ofstream pub(directory / L"org.pub", std::ios::binary | std::ios::trunc);
        if (!pub.is_open()) {
            return false;
        }
        pub.write(reinterpret_cast<const char*>(public_key.data()),
                  static_cast<std::streamsize>(public_key.size()));
    }

    {
        std::ofstream key(directory / L"org.key", std::ios::binary | std::ios::trunc);
        if (!key.is_open()) {
            return false;
        }
        key.write(reinterpret_cast<const char*>(private_key.data()),
                   static_cast<std::streamsize>(private_key.size()));
    }
    return true;
}

AuthGuard::AuthGuard() {
    load_org_public_key();
}

bool AuthGuard::load_org_public_key() {
    std::lock_guard lock(mutex_);
    org_key_loaded_ = load_org_public_key_file(resolve_org_public_key_path(), org_public_key_);
    return org_key_loaded_;
}

std::uint64_t AuthGuard::now_unix() const {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

AuthChallenge AuthGuard::create_challenge(AuthAction action, const std::string& host) {
    AuthChallenge challenge{};
    challenge.expiry_unix = now_unix() + 300;
    challenge.action = action;

    const std::string host_field = host.size() >= kAuthHostFieldSize
                                       ? host.substr(0, kAuthHostFieldSize - 1)
                                       : host;
    std::memcpy(challenge.host, host_field.c_str(), host_field.size());

    if (BCryptGenRandom(nullptr, challenge.nonce.data(),
                        static_cast<ULONG>(challenge.nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) !=
        0) {
        return challenge;
    }
    return challenge;
}

bool AuthGuard::submit_token(const AuthToken& token) {
    std::lock_guard lock(mutex_);
    if (!org_key_loaded_) {
        return false;
    }
    if (token.challenge.expiry_unix < now_unix()) {
        return false;
    }
    if (!verify_auth_token(token, org_public_key_)) {
        return false;
    }

    if (token.challenge.action == AuthAction::StopService) {
        stop_authorized_until_ = token.challenge.expiry_unix;
    } else if (token.challenge.action == AuthAction::Uninstall) {
        uninstall_authorized_until_ = token.challenge.expiry_unix;
    }
    return true;
}

bool AuthGuard::is_authorized(AuthAction action) const {
    std::lock_guard lock(mutex_);
    if (!org_key_loaded_) {
#if defined(USERAUDIT_DEV_CONSOLE)
        return true;
#else
        return false;
#endif
    }

    const std::uint64_t now = now_unix();
    if (action == AuthAction::StopService) {
        return stop_authorized_until_ >= now;
    }
    if (action == AuthAction::Uninstall) {
        return uninstall_authorized_until_ >= now;
    }
    return false;
}

void AuthGuard::clear_authorization() {
    std::lock_guard lock(mutex_);
    stop_authorized_until_ = 0;
    uninstall_authorized_until_ = 0;
}

}  // namespace useraudit
