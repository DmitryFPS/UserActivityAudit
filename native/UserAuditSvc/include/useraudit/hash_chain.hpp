#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace useraudit {

class HashChain {
public:
    bool initialize(const std::filesystem::path& state_path, const std::uint8_t* hmac_key,
                    std::size_t hmac_key_size);

    // Adds seq/prev_hmac/hmac fields to JSON payload and updates persisted chain state.
    std::string seal_json_line(const std::string& json_without_chain_fields);

    bool verify_json_line(const std::string& json_line) const;

private:
    std::string compute_hmac(const std::string& prev_hmac, const std::string& json_body) const;
    bool load_state();
    bool save_state() const;

    std::filesystem::path state_path_;
    const std::uint8_t* hmac_key_ = nullptr;
    std::size_t hmac_key_size_ = 0;
    std::uint64_t seq_ = 0;
    std::string prev_hmac_ = "genesis";
};

}  // namespace useraudit
