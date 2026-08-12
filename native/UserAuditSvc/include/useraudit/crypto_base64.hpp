#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace useraudit {

std::string base64_encode(const std::uint8_t* data, std::size_t size);
bool base64_decode(const std::string& input, std::vector<std::uint8_t>& output);

}  // namespace useraudit
