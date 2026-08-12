#pragma once

#include <cstdint>
#include <string>

namespace useraudit {

inline constexpr char kEncryptedLogLinePrefix[] = "v1:";
inline constexpr char kEncryptedLogExtension[] = ".jsonl.enc";
inline constexpr std::size_t kMaxPlainLogLineBytes = 8192;

bool encrypt_log_line(const std::uint8_t* dek, std::size_t dek_size, const std::string& plaintext,
                      std::string& encrypted_line);

bool decrypt_log_line(const std::uint8_t* dek, std::size_t dek_size, const std::string& encrypted_line,
                      std::string& plaintext);

}  // namespace useraudit
