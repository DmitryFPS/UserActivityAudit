#include "useraudit/auth_protocol.hpp"
#include "useraudit/auth_guard.hpp"

#include <windows.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kAuthPipeName[] = L"\\\\.\\pipe\\UserAudit\\Auth";

bool read_exact(HANDLE pipe, void* buffer, DWORD size) {
    DWORD read = 0;
    return ReadFile(pipe, buffer, size, &read, nullptr) && read == size;
}

bool write_exact(HANDLE pipe, const void* buffer, DWORD size) {
    DWORD written = 0;
    return WriteFile(pipe, buffer, size, &written, nullptr) && written == size;
}

bool pipe_transaction(const char* command, const std::vector<std::uint8_t>& request_payload,
                      std::vector<std::uint8_t>& response_payload) {
    HANDLE pipe = CreateFileW(kAuthPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    char command_buffer[32]{};
    std::memcpy(command_buffer, command, std::min<std::size_t>(sizeof(command_buffer) - 1, std::strlen(command)));

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

    if (!write_exact(pipe, command_buffer, sizeof(command_buffer))) {
        CloseHandle(pipe);
        return false;
    }

    if (!request_payload.empty()) {
        const DWORD size = static_cast<DWORD>(request_payload.size());
        if (!write_exact(pipe, &size, sizeof(size)) ||
            !write_exact(pipe, request_payload.data(), size)) {
            CloseHandle(pipe);
            return false;
        }
    }

    DWORD size = 0;
    if (!read_exact(pipe, &size, sizeof(size)) || size > 8192) {
        CloseHandle(pipe);
        return false;
    }

    response_payload.assign(size, 0);
    if (size > 0 && !read_exact(pipe, response_payload.data(), size)) {
        CloseHandle(pipe);
        return false;
    }

    CloseHandle(pipe);
    return true;
}

bool parse_challenge_bytes(const std::vector<std::uint8_t>& bytes,
                           useraudit::AuthChallenge& challenge) {
    if (bytes.size() < 8 + useraudit::kAuthChallengeNonceSize + 1 + useraudit::kAuthHostFieldSize) {
        return false;
    }

    std::size_t offset = 0;
    challenge.expiry_unix = 0;
    for (int i = 0; i < 8; ++i) {
        challenge.expiry_unix |= static_cast<std::uint64_t>(bytes[offset++]) << (8 * i);
    }
    std::memcpy(challenge.nonce.data(), bytes.data() + offset, useraudit::kAuthChallengeNonceSize);
    offset += useraudit::kAuthChallengeNonceSize;
    challenge.action = static_cast<useraudit::AuthAction>(bytes[offset++]);
    std::memcpy(challenge.host, bytes.data() + offset, useraudit::kAuthHostFieldSize);
    return true;
}

bool fetch_challenge(useraudit::AuthAction action, useraudit::AuthChallenge& challenge) {
    const char* command =
        action == useraudit::AuthAction::Uninstall ? "GET_CHALLENGE_UNINSTALL" : "GET_CHALLENGE";
    std::vector<std::uint8_t> response;
    if (!pipe_transaction(command, {}, response)) {
        return false;
    }
    return parse_challenge_bytes(response, challenge);
}

bool submit_token(const useraudit::AuthToken& token) {
    const auto bytes = useraudit::serialize_auth_token(token);
    std::vector<std::uint8_t> response;
    if (!pipe_transaction("SUBMIT_TOKEN", bytes, response)) {
        return false;
    }
    return !response.empty() && response[0] == 1;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::filesystem::path key_path;
    bool do_stop = false;
    bool do_uninstall = false;

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--sign-stop") {
            do_stop = true;
        } else if (arg == L"--uninstall") {
            do_uninstall = true;
        } else if (arg == L"--key" && i + 1 < argc) {
            key_path = argv[++i];
        }
    }

    if (key_path.empty() || (!do_stop && !do_uninstall)) {
        std::wcerr << L"Usage:\n"
                   << L"  UserAuditAdmin.exe --sign-stop --key E:\\org.key\n"
                   << L"  UserAuditAdmin.exe --uninstall --key E:\\org.key\n";
        return 1;
    }

    std::array<std::uint8_t, useraudit::kEd25519PrivateKeySize> private_key{};
    if (!useraudit::load_org_private_key_file(key_path, private_key)) {
        std::wcerr << L"Failed to read private key: " << key_path.wstring() << L"\n";
        return 1;
    }

    const useraudit::AuthAction action =
        do_uninstall ? useraudit::AuthAction::Uninstall : useraudit::AuthAction::StopService;

    useraudit::AuthChallenge challenge{};
    if (!fetch_challenge(action, challenge)) {
        std::wcerr << L"Failed to fetch auth challenge from UserAuditSvc pipe.\n";
        return 1;
    }

    useraudit::AuthToken token{};
    token.challenge = challenge;
    useraudit::sign_auth_token(token, private_key);

    if (!submit_token(token)) {
        std::wcerr << L"Auth token rejected by service.\n";
        return 1;
    }

    std::wcout << L"IT authorization granted.\n";
    if (do_stop) {
        std::wcout << L"Run: sc stop UserAuditSvc\n";
        return 0;
    }

    std::wcout << L"Run: UserAudit.exe --uninstall\n";
    return 0;
}
