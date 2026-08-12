#include "useraudit/upload_client.hpp"

#include "useraudit/config_manager.hpp"

#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect_true(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expect_eq(int actual, int expected, const char* message) {
    if (actual != expected) {
        ++failures;
        std::cerr << "FAIL: " << message << " (expected " << expected << ", got " << actual
                  << ")\n";
    }
}

}  // namespace

int main() {
    using useraudit::AgentConfig;
    using useraudit::UploadMode;
    using useraudit::parse_agent_config_json;
    using useraudit::parse_upload_settings;

    AgentConfig config{};
    std::string error;
    const std::string json = R"({
  "version": 1,
  "profile": "standard",
  "server": {
    "ingest_url": "https://127.0.0.1:8443/mock",
    "upload_interval_minutes": 45
  }
})";

    expect_true(parse_agent_config_json(json, config, error), "parse server section");
    expect_eq(config.upload_interval_minutes, 45, "upload interval");
    const auto settings = parse_upload_settings(config);
    expect_eq(static_cast<int>(settings.mode), static_cast<int>(UploadMode::MockOutbox),
              "mock mode");

    config.ingest_url = "https://audit.example.com:8443";
    const auto http_settings = parse_upload_settings(config);
    expect_eq(static_cast<int>(http_settings.mode), static_cast<int>(UploadMode::Http),
              "http mode");

    config.ingest_url.clear();
    const auto disabled = parse_upload_settings(config);
    expect_eq(static_cast<int>(disabled.mode), static_cast<int>(UploadMode::Disabled),
              "disabled mode");

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
