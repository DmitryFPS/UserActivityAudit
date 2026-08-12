#include "useraudit/config_manager.hpp"

#include <cstdlib>
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
    using useraudit::AuditProfile;
    using useraudit::detect_profile_from_ram_mb;
    using useraudit::expand_config_path;
    using useraudit::parse_agent_config_json;

    expect_eq(static_cast<int>(detect_profile_from_ram_mb(2048)), static_cast<int>(AuditProfile::Low),
              "2 GB RAM -> Low");
    expect_eq(static_cast<int>(detect_profile_from_ram_mb(8192)),
              static_cast<int>(AuditProfile::Standard), "8 GB RAM -> Standard");

    const std::string json = R"({
  "version": 1,
  "profile": "full",
  "collectors": {
    "session": true,
    "process": true,
    "foreground": true,
    "usb": true,
    "file": true,
    "network": true,
    "clipboard": true,
    "print": false
  },
  "paths": {
    "critical": ["%USERPROFILE%\\Documents"],
    "sensitive": []
  },
  "storage": {
    "max_log_mb_per_day": 25
  }
})";

    AgentConfig config{};
    std::string error;
    expect_true(parse_agent_config_json(json, config, error), "parse config json");
    expect_eq(static_cast<int>(config.profile), static_cast<int>(AuditProfile::Full), "profile full");
    expect_eq(config.network_poll_sec, 15, "full network poll");
    expect_true(config.collectors.clipboard, "clipboard enabled");
    expect_true(!config.collectors.print, "print disabled");
    expect_eq(static_cast<int>(config.max_log_mb_per_day), 50, "full max log clamp");

    const auto expanded = expand_config_path(L"%SystemRoot%\\Temp");
    expect_true(!expanded.empty(), "expand env path");

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
