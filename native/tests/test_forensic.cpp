#include "useraudit/browser_parser.hpp"
#include "useraudit/zip_writer.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

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
    using useraudit::ZipEntry;
    using useraudit::chrome_timestamp_to_iso8601;
    using useraudit::create_zip_store;
    using useraudit::unix_microseconds_to_iso8601;

    const auto chrome_ts = chrome_timestamp_to_iso8601(13390000000000000LL);
    expect_true(!chrome_ts.empty() && chrome_ts.find('T') != std::string::npos,
                "chrome timestamp converts");

    const auto unix_ts = unix_microseconds_to_iso8601(1700000000000000LL);
    expect_true(!unix_ts.empty(), "unix microseconds converts");

    const auto temp_dir = std::filesystem::temp_directory_path() / "useraudit-zip-test";
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
    std::filesystem::create_directories(temp_dir, ec);

    const auto source = temp_dir / "hello.txt";
    {
        std::ofstream file(source, std::ios::trunc);
        file << "forensic zip test";
    }

    const auto zip_path = temp_dir / "test.zip";
    std::string error;
    expect_true(create_zip_store(zip_path, {ZipEntry{source, "hello.txt"}}, error),
                "create zip store");
    expect_true(std::filesystem::exists(zip_path), "zip file exists");

    std::filesystem::remove_all(temp_dir, ec);

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
