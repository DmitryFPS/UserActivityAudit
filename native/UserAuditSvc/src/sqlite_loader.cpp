#include "useraudit/sqlite_loader.hpp"

#include <windows.h>

namespace useraudit {

namespace {

template <typename T>
T load_symbol(void* module, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(module), name));
}

}  // namespace

SqliteLoader::SqliteLoader() {
    module_ = LoadLibraryW(L"winsqlite3.dll");
    if (module_ == nullptr) {
        return;
    }

    open_ = load_symbol<open_fn>(module_, "sqlite3_open_v2");
    close_ = load_symbol<close_fn>(module_, "sqlite3_close");
    prepare_ = load_symbol<prepare_fn>(module_, "sqlite3_prepare_v2");
    step_ = load_symbol<step_fn>(module_, "sqlite3_step");
    finalize_ = load_symbol<finalize_fn>(module_, "sqlite3_finalize");
    column_text_ = load_symbol<column_text_fn>(module_, "sqlite3_column_text");
    column_int64_ = load_symbol<column_int64_fn>(module_, "sqlite3_column_int64");
    load_symbol<errmsg_fn>(module_, "sqlite3_errmsg");

    available_ = open_ != nullptr && close_ != nullptr && prepare_ != nullptr && step_ != nullptr &&
                 finalize_ != nullptr && column_text_ != nullptr && column_int64_ != nullptr;
}

SqliteLoader::~SqliteLoader() {
    if (module_ != nullptr) {
        FreeLibrary(static_cast<HMODULE>(module_));
        module_ = nullptr;
    }
}

bool SqliteLoader::open_readonly(const std::wstring& path, sqlite3** db, std::string& error) const {
    if (!available_ || db == nullptr) {
        error = "winsqlite3.dll unavailable";
        return false;
    }

    const int size =
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), static_cast<int>(path.size()), nullptr, 0,
                            nullptr, nullptr);
    if (size <= 0) {
        error = "invalid sqlite path encoding";
        return false;
    }
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), static_cast<int>(path.size()), utf8.data(), size,
                        nullptr, nullptr);

    constexpr int kReadOnly = 0x00000001;
    if (open_(utf8.c_str(), db, kReadOnly) != 0) {
        error = "sqlite3_open_v2 failed";
        return false;
    }
    error.clear();
    return true;
}

void SqliteLoader::close(sqlite3* db) const {
    if (available_ && db != nullptr) {
        close_(db);
    }
}

bool SqliteLoader::prepare(sqlite3* db, const char* sql, sqlite3_stmt** stmt,
                           std::string& error) const {
    if (!available_ || db == nullptr || sql == nullptr || stmt == nullptr) {
        error = "sqlite prepare unavailable";
        return false;
    }
    if (prepare_(db, sql, -1, stmt, nullptr) != 0) {
        error = "sqlite3_prepare_v2 failed";
        return false;
    }
    error.clear();
    return true;
}

int SqliteLoader::step(sqlite3_stmt* stmt) const {
    return available_ && stmt != nullptr ? step_(stmt) : -1;
}

void SqliteLoader::finalize(sqlite3_stmt* stmt) const {
    if (available_ && stmt != nullptr) {
        finalize_(stmt);
    }
}

const char* SqliteLoader::column_text(sqlite3_stmt* stmt, int index) const {
    return available_ && stmt != nullptr ? column_text_(stmt, index) : nullptr;
}

long long SqliteLoader::column_int64(sqlite3_stmt* stmt, int index) const {
    return available_ && stmt != nullptr ? column_int64_(stmt, index) : 0;
}

}  // namespace useraudit
