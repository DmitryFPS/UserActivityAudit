#include "useraudit/usb_helpers.hpp"

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

void expect_eq(const std::string& actual, const std::string& expected, const char* message) {
    if (actual != expected) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n  expected: " << expected << "\n  actual:   "
                  << actual << '\n';
    }
}

}  // namespace

int main() {
    useraudit::UsbVolumeIdentity identity{};

    expect_true(useraudit::parse_usb_vid_pid(
                    "USB\\VID_0411&PID_0397\\0123456789ABCDEF", identity),
                "parse valid pnp id");
    expect_eq(identity.vid, "0411", "vid");
    expect_eq(identity.pid, "0397", "pid");
    expect_eq(identity.serial, "0123456789ABCDEF", "serial");

    expect_true(!useraudit::parse_usb_vid_pid("SCSI\\DISK&VEN_WDC", identity),
                "reject non-usb id");

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
