#include "useraudit/crypto_base64.hpp"

#include <windows.h>
#include <wincrypt.h>

namespace useraudit {

std::string base64_encode(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return {};
    }

    DWORD encoded_size = 0;
    if (!CryptBinaryToStringA(data, static_cast<DWORD>(size), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              nullptr, &encoded_size)) {
        return {};
    }

    std::string encoded(encoded_size, '\0');
    if (!CryptBinaryToStringA(data, static_cast<DWORD>(size), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              encoded.data(), &encoded_size)) {
        return {};
    }

    while (!encoded.empty() && (encoded.back() == '\0' || encoded.back() == '\r' || encoded.back() == '\n')) {
        encoded.pop_back();
    }
    return encoded;
}

bool base64_decode(const std::string& input, std::vector<std::uint8_t>& output) {
    if (input.empty()) {
        return false;
    }

    DWORD decoded_size = 0;
    if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()), CRYPT_STRING_BASE64,
                              nullptr, &decoded_size, nullptr, nullptr)) {
        return false;
    }

    output.resize(decoded_size);
    if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()), CRYPT_STRING_BASE64,
                              output.data(), &decoded_size, nullptr, nullptr)) {
        output.clear();
        return false;
    }

    output.resize(decoded_size);
    return true;
}

}  // namespace useraudit
