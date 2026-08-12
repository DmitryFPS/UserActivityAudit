#include "useraudit/time_utils.hpp"

#include <windows.h>
#include <rpc.h>

#include <array>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <string_view>

namespace useraudit {

namespace {

std::string format_system_time_utc(const SYSTEMTIME& st, int millis) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << st.wYear << '-' << std::setw(2) << st.wMonth
        << '-' << std::setw(2) << st.wDay << 'T' << std::setw(2) << st.wHour << ':'
        << std::setw(2) << st.wMinute << ':' << std::setw(2) << st.wSecond << '.'
        << std::setw(3) << millis << 'Z';
    return oss.str();
}

std::wstring_view find_attribute(std::wstring_view xml, std::wstring_view attr_name) {
    const std::wstring pattern = std::wstring(attr_name) + L"='";
    const size_t pos = xml.find(pattern);
    if (pos == std::wstring_view::npos) {
        return {};
    }
    const size_t start = pos + pattern.size();
    const size_t end = xml.find(L'\'', start);
    if (end == std::wstring_view::npos) {
        return {};
    }
    return xml.substr(start, end - start);
}

std::wstring_view extract_xml_value(std::wstring_view xml, std::wstring_view tag) {
    const std::wstring open = L"<" + std::wstring(tag) + L">";
    const std::wstring close = L"</" + std::wstring(tag) + L">";
    const size_t start = xml.find(open);
    if (start == std::wstring_view::npos) {
        return {};
    }
    const size_t value_start = start + open.size();
    const size_t end = xml.find(close, value_start);
    if (end == std::wstring_view::npos) {
        return {};
    }
    return xml.substr(value_start, end - value_start);
}

std::wstring_view extract_data_value(std::wstring_view xml, std::wstring_view name) {
    const std::wstring pattern = L"Name='" + std::wstring(name) + L"'>";
    const size_t pos = xml.find(pattern);
    if (pos == std::wstring_view::npos) {
        return {};
    }
    const size_t value_start = pos + pattern.size();
    const size_t end = xml.find(L"</Data>", value_start);
    if (end == std::wstring_view::npos) {
        return {};
    }
    return xml.substr(value_start, end - value_start);
}

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

}  // namespace

std::string utc_now_iso8601() {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    return format_system_time_utc(st, static_cast<int>(st.wMilliseconds));
}

std::string generate_event_id() {
    UUID uuid{};
    if (UuidCreate(&uuid) != RPC_S_OK) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 15);
        static constexpr char hex[] = "0123456789abcdef";
        std::string fallback = "00000000-0000-4000-8000-";
        for (int i = 0; i < 12; ++i) {
            fallback.push_back(hex[dist(gen)]);
        }
        return fallback;
    }

    RPC_CSTR str = nullptr;
    if (UuidToStringA(&uuid, &str) != RPC_S_OK || str == nullptr) {
        return "00000000-0000-4000-8000-000000000000";
    }
    std::string result(reinterpret_cast<char*>(str));
    RpcStringFreeA(&str);
    return result;
}

std::string iso8601_from_event_xml(const std::wstring& xml) {
    const std::wstring_view xml_view = xml;
    const std::wstring_view system_time = find_attribute(xml_view, L"SystemTime");
    if (system_time.empty()) {
        return utc_now_iso8601();
    }

    SYSTEMTIME st{};
    int millis = 0;
    const std::wstring st_wide(system_time);
    swscanf_s(st_wide.c_str(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu.%3d", &st.wYear, &st.wMonth,
              &st.wDay, &st.wHour, &st.wMinute, &st.wSecond, &millis);
    return format_system_time_utc(st, millis);
}

}  // namespace useraudit
