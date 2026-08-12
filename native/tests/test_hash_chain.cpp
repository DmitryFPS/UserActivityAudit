#include "useraudit/hash_chain.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect_true(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

}  // namespace

int main() {
    using useraudit::HashChain;

    const auto state_path =
        std::filesystem::temp_directory_path() / "useraudit-test-chain.state";

    std::error_code ec;
    std::filesystem::remove(state_path, ec);

    std::array<std::uint8_t, 32> hmac_key{};
    for (std::size_t i = 0; i < hmac_key.size(); ++i) {
        hmac_key[i] = static_cast<std::uint8_t>(0xA0 + i);
    }

    HashChain chain;
    expect_true(chain.initialize(state_path, hmac_key.data(), hmac_key.size()), "initialize chain");

    const std::string json1 =
        R"({"id":"1","ts":"2026-08-12T10:00:00.000Z","lvl":1,"cat":"session","act":"login","sev":"info","host":"NB-01","src":"eventlog"})";
    const std::string sealed1 = chain.seal_json_line(json1);
    expect_true(!sealed1.empty(), "seal first line");
    expect_true(sealed1.find("\"seq\":1") != std::string::npos, "seq increment");
    expect_true(chain.verify_json_line(sealed1), "verify first line");

    const std::string json2 =
        R"({"id":"2","ts":"2026-08-12T10:00:01.000Z","lvl":1,"cat":"process","act":"start","sev":"info","host":"NB-01","src":"etw"})";
    const std::string sealed2 = chain.seal_json_line(json2);
    expect_true(sealed2.find("\"seq\":2") != std::string::npos, "seq increment second");
    expect_true(chain.verify_json_line(sealed2), "verify second line");

    std::string tampered = sealed2;
    const auto hmac_pos = tampered.rfind("\"hmac\":\"");
    expect_true(hmac_pos != std::string::npos, "find hmac field");
    tampered[hmac_pos + 10] = '0';
    expect_true(!chain.verify_json_line(tampered), "detect tampered hmac");

    std::filesystem::remove(state_path, ec);

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
