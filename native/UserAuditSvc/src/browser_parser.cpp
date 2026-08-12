#include "useraudit/browser_parser.hpp"

#include <windows.h>

#include <fstream>
#include <map>
#include <sstream>

namespace useraudit {

namespace {

constexpr long long kWebkitEpochOffsetUs = 11644473600000000LL;

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

bool copy_locked_file(const std::filesystem::path& source, const std::filesystem::path& dest,
                      std::string& error) {
    HANDLE handle = CreateFileW(source.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "cannot open browser db: " + source.string();
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart <= 0) {
        CloseHandle(handle);
        error = "invalid browser db size";
        return false;
    }

    std::vector<std::uint8_t> buffer(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
    CloseHandle(handle);
    if (!ok || read != buffer.size()) {
        error = "cannot read browser db";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(dest.parent_path(), ec);
    std::ofstream output(dest, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error = "cannot write temp browser db";
        return false;
    }
    output.write(reinterpret_cast<const char*>(buffer.data()),
                 static_cast<std::streamsize>(buffer.size()));
    return output.good();
}

bool append_json_line(std::ofstream& output, const std::string& browser, const std::string& kind,
                      const std::map<std::string, std::string>& fields) {
    std::ostringstream line;
    line << "{\"browser\":\"" << json_escape(browser) << "\",\"kind\":\"" << json_escape(kind)
         << '"';
    for (const auto& [key, value] : fields) {
        line << ",\"" << json_escape(key) << "\":\"" << json_escape(value) << '"';
    }
    line << '}';
    output << line.str() << '\n';
    return output.good();
}

bool query_chromium_db(SqliteLoader& sqlite, const std::filesystem::path& db_copy,
                       const std::string& browser, int max_rows, std::ofstream& output,
                       BrowserCollectionResult& result, std::string& error) {
    sqlite3* db = nullptr;
    if (!sqlite.open_readonly(db_copy.wstring(), &db, error)) {
        return false;
    }

    sqlite3_stmt* history_stmt = nullptr;
    const std::string history_sql =
        "SELECT url, title, visit_count, last_visit_time FROM urls "
        "ORDER BY last_visit_time DESC LIMIT " +
        std::to_string(max_rows);

    if (sqlite.prepare(db, history_sql.c_str(), &history_stmt, error)) {
        while (sqlite.step(history_stmt) == 100) {  // SQLITE_ROW
            const char* url = sqlite.column_text(history_stmt, 0);
            const char* title = sqlite.column_text(history_stmt, 1);
            const long long visit_count = sqlite.column_int64(history_stmt, 2);
            const long long last_visit = sqlite.column_int64(history_stmt, 3);

            std::map<std::string, std::string> fields;
            fields["url"] = url != nullptr ? url : "";
            fields["title"] = title != nullptr ? title : "";
            fields["visit_count"] = std::to_string(visit_count);
            fields["last_visit_utc"] = chrome_timestamp_to_iso8601(last_visit);
            if (append_json_line(output, browser, "history", fields)) {
                ++result.history_rows;
            }
        }
        sqlite.finalize(history_stmt);
    }

    sqlite3_stmt* download_stmt = nullptr;
    const std::string download_sql =
        "SELECT target_path, tab_url, start_time FROM downloads "
        "ORDER BY start_time DESC LIMIT " +
        std::to_string(max_rows);

    if (sqlite.prepare(db, download_sql.c_str(), &download_stmt, error)) {
        while (sqlite.step(download_stmt) == 100) {
            const char* target = sqlite.column_text(download_stmt, 0);
            const char* tab_url = sqlite.column_text(download_stmt, 1);
            const long long start_time = sqlite.column_int64(download_stmt, 2);

            std::map<std::string, std::string> fields;
            fields["target_path"] = target != nullptr ? target : "";
            fields["tab_url"] = tab_url != nullptr ? tab_url : "";
            fields["start_time_utc"] = chrome_timestamp_to_iso8601(start_time);
            if (append_json_line(output, browser, "download", fields)) {
                ++result.download_rows;
            }
        }
        sqlite.finalize(download_stmt);
    }

    sqlite.close(db);
    error.clear();
    return true;
}

std::filesystem::path profile_local_appdata(const std::filesystem::path& user_root) {
    return user_root / L"AppData" / L"Local";
}

void collect_chromium_profiles(const std::filesystem::path& local_appdata,
                               const std::filesystem::path& vendor_path,
                               const std::string& browser_name, const ForensicSettings& settings,
                               SqliteLoader& sqlite, const std::filesystem::path& browser_dir,
                               int max_rows, BrowserCollectionResult& aggregate,
                               std::string& error) {
    const std::filesystem::path user_data = local_appdata / vendor_path / L"User Data";
    const std::filesystem::path history = user_data / L"Default" / L"History";
    if (!std::filesystem::exists(history)) {
        return;
    }

    const auto temp_copy = browser_dir / (browser_name + "_History.db");
    if (!copy_locked_file(history, temp_copy, error)) {
        return;
    }

    const auto output_path = browser_dir / (browser_name + ".jsonl");
    std::ofstream output(output_path, std::ios::app);
    if (!output.is_open()) {
        error = "cannot open browser output jsonl";
        return;
    }

    BrowserCollectionResult partial{};
    partial.output_jsonl = output_path;
    if (query_chromium_db(sqlite, temp_copy, browser_name, max_rows, output, partial, error)) {
        aggregate.history_rows += partial.history_rows;
        aggregate.download_rows += partial.download_rows;
        aggregate.output_jsonl = output_path;
    }

    std::error_code ec;
    std::filesystem::remove(temp_copy, ec);
}

std::filesystem::path find_firefox_history(const std::filesystem::path& local_appdata) {
    const std::filesystem::path profiles_ini = local_appdata / L"Mozilla" / L"Firefox" / L"profiles.ini";
    if (!std::filesystem::exists(profiles_ini)) {
        return {};
    }

    std::ifstream input(profiles_ini);
    std::string line;
    std::filesystem::path profiles_root = local_appdata / L"Mozilla" / L"Firefox";
    std::filesystem::path default_profile;

    while (std::getline(input, line)) {
        if (line.rfind("Path=", 0) == 0) {
            const std::string relative = line.substr(5);
            const auto candidate = profiles_root / utf8_to_wide(relative) / L"places.sqlite";
            if (std::filesystem::exists(candidate)) {
                default_profile = candidate;
                break;
            }
        }
    }
    return default_profile;
}

bool query_firefox_db(SqliteLoader& sqlite, const std::filesystem::path& db_copy, int max_rows,
                      std::ofstream& output, BrowserCollectionResult& result, std::string& error) {
    sqlite3* db = nullptr;
    if (!sqlite.open_readonly(db_copy.wstring(), &db, error)) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT url, title, visit_count, last_visit_date FROM moz_places "
        "WHERE last_visit_date IS NOT NULL ORDER BY last_visit_date DESC LIMIT " +
        std::to_string(max_rows);

    if (sqlite.prepare(db, sql.c_str(), &stmt, error)) {
        while (sqlite.step(stmt) == 100) {
            const char* url = sqlite.column_text(stmt, 0);
            const char* title = sqlite.column_text(stmt, 1);
            const long long visit_count = sqlite.column_int64(stmt, 2);
            const long long last_visit = sqlite.column_int64(stmt, 3);

            std::map<std::string, std::string> fields;
            fields["url"] = url != nullptr ? url : "";
            fields["title"] = title != nullptr ? title : "";
            fields["visit_count"] = std::to_string(visit_count);
            fields["last_visit_utc"] = unix_microseconds_to_iso8601(last_visit);
            if (append_json_line(output, "firefox", "history", fields)) {
                ++result.history_rows;
            }
        }
        sqlite.finalize(stmt);
    }

    sqlite.close(db);
    error.clear();
    return true;
}

}  // namespace

std::string chrome_timestamp_to_iso8601(long long webkit_microseconds) {
    if (webkit_microseconds <= 0) {
        return {};
    }

    const long long unix_seconds = (webkit_microseconds - kWebkitEpochOffsetUs) / 1000000LL;
    return unix_microseconds_to_iso8601(unix_seconds * 1000000LL);
}

std::string unix_microseconds_to_iso8601(long long unix_microseconds) {
    if (unix_microseconds <= 0) {
        return {};
    }

    const long long unix_seconds = unix_microseconds / 1000000LL;
    if (unix_seconds <= 0) {
        return {};
    }

    FILETIME ft{};
    ULARGE_INTEGER time{};
    time.QuadPart = static_cast<unsigned long long>(unix_seconds) * 10000000ULL + 116444736000000000ULL;
    ft.dwLowDateTime = time.LowPart;
    ft.dwHighDateTime = time.HighPart;

    SYSTEMTIME st_utc{};
    FileTimeToSystemTime(&ft, &st_utc);

    char buffer[32]{};
    sprintf_s(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02uZ", st_utc.wYear, st_utc.wMonth,
              st_utc.wDay, st_utc.wHour, st_utc.wMinute, st_utc.wSecond);
    return buffer;
}

BrowserCollectionResult collect_browser_artifacts(const ForensicSettings& settings,
                                                  const std::filesystem::path& staging_dir,
                                                  SqliteLoader& sqlite, std::string& error) {
    BrowserCollectionResult result{};
    if (!sqlite.available()) {
        error = "winsqlite3.dll unavailable";
        return result;
    }

    const auto browser_dir = staging_dir / "browser";
    std::error_code ec;
    std::filesystem::create_directories(browser_dir, ec);

    const int max_rows = settings.max_browser_rows > 0 ? settings.max_browser_rows : 500;

    const std::wstring users_root = L"C:\\Users\\*";
    WIN32_FIND_DATAW entry{};
    HANDLE handle = FindFirstFileW(users_root.c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "cannot enumerate user profiles";
        return result;
    }

    do {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        if (entry.cFileName[0] == L'.') {
            continue;
        }
        if (_wcsicmp(entry.cFileName, L"Public") == 0 || _wcsicmp(entry.cFileName, L"Default") == 0 ||
            _wcsicmp(entry.cFileName, L"Default User") == 0 ||
            _wcsicmp(entry.cFileName, L"All Users") == 0) {
            continue;
        }

        const std::filesystem::path user_root = std::filesystem::path(L"C:\\Users") / entry.cFileName;
        const auto local_appdata = profile_local_appdata(user_root);

        if (settings.browser_chrome) {
            collect_chromium_profiles(local_appdata, std::filesystem::path(L"Google") / L"Chrome",
                                     "chrome", settings, sqlite, browser_dir, max_rows, result,
                                     error);
        }
        if (settings.browser_edge) {
            collect_chromium_profiles(local_appdata, std::filesystem::path(L"Microsoft") / L"Edge",
                                     "edge", settings, sqlite, browser_dir, max_rows, result,
                                     error);
        }
        if (settings.browser_firefox) {
            const auto firefox_db = find_firefox_history(local_appdata);
            if (!firefox_db.empty()) {
                const auto temp_copy = browser_dir / "firefox_places.db";
                if (copy_locked_file(firefox_db, temp_copy, error)) {
                    const auto output_path = browser_dir / "firefox.jsonl";
                    std::ofstream output(output_path, std::ios::app);
                    if (output.is_open()) {
                        BrowserCollectionResult partial{};
                        if (query_firefox_db(sqlite, temp_copy, max_rows, output, partial, error)) {
                            result.history_rows += partial.history_rows;
                            result.output_jsonl = output_path;
                        }
                    }
                    std::filesystem::remove(temp_copy, ec);
                }
            }
        }
    } while (FindNextFileW(handle, &entry));

    FindClose(handle);
    error.clear();
    return result;
}

}  // namespace useraudit
