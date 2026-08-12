#include "useraudit/auth_guard.hpp"

extern "C" {
#include "ed25519.h"
}

#include <windows.h>

#include <array>
#include <filesystem>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    std::filesystem::path output_directory;
    for (int i = 1; i < argc; ++i) {
        if (std::wstring(argv[i]) == L"--out" && i + 1 < argc) {
            output_directory = argv[++i];
        }
    }

    if (output_directory.empty()) {
        std::wcerr << L"Usage: UserAuditKeygen.exe --out D:\\IT\\\n";
        return 1;
    }

    std::array<std::uint8_t, useraudit::kEd25519PublicKeySize> public_key{};
    std::array<std::uint8_t, useraudit::kEd25519PrivateKeySize> private_key{};
    std::array<std::uint8_t, 32> seed{};

    if (ed25519_create_seed(seed.data()) != 0) {
        std::wcerr << L"Failed to generate random seed.\n";
        return 1;
    }

    ed25519_create_keypair(public_key.data(), private_key.data(), seed.data());
    std::memcpy(private_key.data() + 32, public_key.data(), 32);

    if (!useraudit::save_org_keypair(output_directory, public_key, private_key)) {
        std::wcerr << L"Failed to write keypair to " << output_directory.wstring() << L"\n";
        return 1;
    }

    const auto pub_path = output_directory / L"org.pub";
    if (!useraudit::load_org_public_key_file(pub_path, public_key)) {
        std::wcerr << L"Key verification failed.\n";
        return 1;
    }

    std::wcout << L"Org keypair written to " << output_directory.wstring() << L"\n";
    std::wcout << L"  org.key — keep on IT USB only\n";
    std::wcout << L"  org.pub — deploy to %ProgramData%\\UserAudit\\keys\\org.pub on fleet\n";
    return 0;
}
