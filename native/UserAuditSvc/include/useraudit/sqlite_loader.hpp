#pragma once

#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace useraudit {

class SqliteLoader {
public:
    SqliteLoader();
    ~SqliteLoader();

    SqliteLoader(const SqliteLoader&) = delete;
    SqliteLoader& operator=(const SqliteLoader&) = delete;

    [[nodiscard]] bool available() const { return available_; }

    [[nodiscard]] bool open_readonly(const std::wstring& path, sqlite3** db, std::string& error) const;
    void close(sqlite3* db) const;

    [[nodiscard]] bool prepare(sqlite3* db, const char* sql, sqlite3_stmt** stmt, std::string& error) const;
    [[nodiscard]] int step(sqlite3_stmt* stmt) const;
    void finalize(sqlite3_stmt* stmt) const;

    [[nodiscard]] const char* column_text(sqlite3_stmt* stmt, int index) const;
    [[nodiscard]] long long column_int64(sqlite3_stmt* stmt, int index) const;

private:
    bool available_ = false;
    void* module_ = nullptr;

    using open_fn = int(__cdecl*)(const char*, sqlite3**, int);
    using close_fn = int(__cdecl*)(sqlite3*);
    using prepare_fn = int(__cdecl*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
    using step_fn = int(__cdecl*)(sqlite3_stmt*);
    using finalize_fn = int(__cdecl*)(sqlite3_stmt*);
    using column_text_fn = const char*(__cdecl*)(sqlite3_stmt*, int);
    using column_int64_fn = long long(__cdecl*)(sqlite3_stmt*, int);
    using errmsg_fn = const char*(__cdecl*)(sqlite3*);

    open_fn open_ = nullptr;
    close_fn close_ = nullptr;
    prepare_fn prepare_ = nullptr;
    step_fn step_ = nullptr;
    finalize_fn finalize_ = nullptr;
    column_text_fn column_text_ = nullptr;
    column_int64_fn column_int64_ = nullptr;
    errmsg_fn errmsg_ = nullptr;
};

}  // namespace useraudit
