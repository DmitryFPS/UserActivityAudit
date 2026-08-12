#include "useraudit/event_serializer.hpp"

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

void expect_eq(const std::string& actual, const std::string& expected, const char* message) {
    if (actual != expected) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n  expected: " << expected << "\n  actual:   "
                  << actual << '\n';
    }
}

}  // namespace

int main() {
    using useraudit::AuditEvent;
    using useraudit::json_escape;
    using useraudit::serialize_event_json;

    expect_eq(json_escape("hello"), "\"hello\"", "json_escape plain");
    expect_eq(json_escape("a\"b\\c"), "\"a\\\"b\\\\c\"", "json_escape special chars");

    AuditEvent event;
    event.id = "11111111-1111-4111-8111-111111111111";
    event.ts = "2026-08-12T10:00:00.000Z";
    event.lvl = 1;
    event.cat = "session";
    event.act = "login";
    event.sev = "info";
    event.host = "NB-01";
    event.user = "DOMAIN\\user";
    event.src = "eventlog";
    event.data["event_id"] = "4624";
    event.data["logon_type"] = "2";

    const std::string json = serialize_event_json(event);
    expect_true(json.find("\"cat\":\"session\"") != std::string::npos, "category session");
    expect_true(json.find("\"act\":\"login\"") != std::string::npos, "action login");
    expect_true(json.find("\"user\":\"DOMAIN\\\\user\"") != std::string::npos,
                "escaped backslash in user");
    expect_true(json.find("\"event_id\":\"4624\"") != std::string::npos, "data event_id");

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
