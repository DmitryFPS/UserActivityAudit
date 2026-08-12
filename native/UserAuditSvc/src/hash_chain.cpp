#include "useraudit/hash_chain.hpp"

#include <bcrypt.h>

#include <fstream>

namespace useraudit {

namespace {

std::string bytes_to_hex(const std::uint8_t* data, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(kHex[(data[i] >> 4) & 0x0F]);
        out.push_back(kHex[data[i] & 0x0F]);
    }
    return out;
}

}  // namespace

bool HashChain::initialize(const std::filesystem::path& state_path, const std::uint8_t* hmac_key,
                           std::size_t hmac_key_size) {
    state_path_ = state_path;
    hmac_key_ = hmac_key;
    hmac_key_size_ = hmac_key_size;
    return hmac_key_ != nullptr && hmac_key_size_ == 32 && load_state();
}

bool HashChain::load_state() {
    std::ifstream input(state_path_);
    if (!input.is_open()) {
        seq_ = 0;
        prev_hmac_ = "genesis";
        return true;
    }

    std::uint64_t seq = 0;
    std::string prev;
    if (!(input >> seq)) {
        seq_ = 0;
        prev_hmac_ = "genesis";
        return true;
    }
    input >> prev;
    seq_ = seq;
    prev_hmac_ = prev.empty() ? "genesis" : prev;
    return true;
}

bool HashChain::save_state() const {
    std::error_code ec;
    std::filesystem::create_directories(state_path_.parent_path(), ec);

    std::ofstream output(state_path_, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << seq_ << ' ' << prev_hmac_;
    return output.good();
}

std::string HashChain::compute_hmac(const std::string& prev_hmac,
                                    const std::string& json_body) const {
    if (hmac_key_ == nullptr || hmac_key_size_ != 32) {
        return {};
    }

    std::string material = prev_hmac;
    material.push_back('\n');
    material += json_body;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                                  BCRYPT_ALG_HANDLE_HMAC_FLAG))) {
        return {};
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0,
                                         const_cast<PUCHAR>(hmac_key_),
                                         static_cast<ULONG>(hmac_key_size_), 0))) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    if (!BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(material.data()),
                                       static_cast<ULONG>(material.size()), 0))) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::uint8_t digest[32]{};
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0))) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return bytes_to_hex(digest, sizeof(digest));
}

std::string HashChain::seal_json_line(const std::string& json_without_chain_fields) {
    if (json_without_chain_fields.empty() || json_without_chain_fields.front() != '{' ||
        json_without_chain_fields.back() != '}') {
        return {};
    }

    ++seq_;
    const std::string hmac = compute_hmac(prev_hmac_, json_without_chain_fields);
    if (hmac.empty()) {
        --seq_;
        return {};
    }

    std::string sealed = json_without_chain_fields;
    sealed.pop_back();
    sealed += ",\"seq\":" + std::to_string(seq_);
    sealed += ",\"prev_hmac\":\"" + prev_hmac_ + "\"";
    sealed += ",\"hmac\":\"" + hmac + "\"}";

    prev_hmac_ = hmac;
    save_state();
    return sealed;
}

bool HashChain::verify_json_line(const std::string& json_line) const {
    const auto hmac_pos = json_line.rfind("\"hmac\":\"");
    const auto prev_pos = json_line.rfind("\"prev_hmac\":\"");
    if (hmac_pos == std::string::npos || prev_pos == std::string::npos) {
        return false;
    }

    const auto hmac_start = hmac_pos + 8;
    const auto hmac_end = json_line.find('"', hmac_start);
    if (hmac_end == std::string::npos) {
        return false;
    }

    const auto prev_start = prev_pos + 13;
    const auto prev_end = json_line.find('"', prev_start);
    if (prev_end == std::string::npos) {
        return false;
    }

    const std::string expected_hmac = json_line.substr(hmac_start, hmac_end - hmac_start);
    const std::string prev_hmac = json_line.substr(prev_start, prev_end - prev_start);

    const auto body_end = json_line.find(",\"seq\":");
    if (body_end == std::string::npos) {
        return false;
    }

    const std::string body = json_line.substr(0, body_end) + "}";
    const std::string actual = compute_hmac(prev_hmac, body);
    return !actual.empty() && actual == expected_hmac;
}

}  // namespace useraudit
