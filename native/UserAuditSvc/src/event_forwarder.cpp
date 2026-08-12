#include "useraudit/event_forwarder.hpp"

#include "useraudit/event_serializer.hpp"

#include <windows.h>
#include <winhttp.h>

#include <sstream>
#include <thread>

namespace useraudit {

namespace {

URL_COMPONENTS parse_url(std::wstring url) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    WinHttpCrackUrl(url.c_str(), 0, 0, &parts);
    return parts;
}

std::wstring component_to_string(const std::wstring& url, const WCHAR* ptr, DWORD length) {
    if (ptr == nullptr || length == 0) {
        return {};
    }
    return url.substr(static_cast<size_t>(ptr - url.c_str()), length);
}

std::string build_payload_json(const AuditEvent& event) {
    if (event.data.empty()) {
        return {};
    }
    std::ostringstream out;
    out << '{';
    bool first = true;
    for (const auto& [key, value] : event.data) {
        if (!first) {
            out << ',';
        }
        first = false;
        out << json_escape(key) << ':' << json_escape(value);
    }
    out << '}';
    return out.str();
}

std::string build_ingest_dto_json(const AuditEvent& event) {
    const std::string payload = build_payload_json(event);
    std::ostringstream out;
    out << '{'
        << "\"EventId\":" << json_escape(event.id) << ','
        << "\"Timestamp\":" << json_escape(event.ts) << ','
        << "\"Category\":" << json_escape(event.cat) << ','
        << "\"Action\":" << json_escape(event.act) << ','
        << "\"Severity\":" << json_escape(event.sev) << ','
        << "\"Host\":" << json_escape(event.host) << ','
        << "\"PayloadJson\":";
    if (payload.empty()) {
        out << "null";
    } else {
        out << json_escape(payload);
    }
    out << '}';
    return out.str();
}

bool post_json(const std::string& base_url, const std::string& body) {
    const std::wstring wide_url(base_url.begin(), base_url.end());
    URL_COMPONENTS parts = parse_url(wide_url);
    const std::wstring host = component_to_string(wide_url, parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring url_path =
        component_to_string(wide_url, parts.lpszUrlPath, parts.dwUrlPathLength);
    const std::wstring request_path =
        url_path.empty() ? L"/api/v1/ingest/events" : url_path + L"/api/v1/ingest/events";

    HINTERNET session =
        WinHttpOpen(L"UserAuditAgent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        return false;
    }

    const INTERNET_PORT port =
        parts.nPort != 0 ? parts.nPort
                         : (parts.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
                                                                   : INTERNET_DEFAULT_HTTP_PORT);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (connect == nullptr) {
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD flags =
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", request_path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           flags);
    if (request == nullptr) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const std::wstring host_header = L"Host: " + host;
    WinHttpAddRequestHeaders(request, host_header.c_str(), static_cast<DWORD>(-1),
                             WINHTTP_ADDREQ_FLAG_ADD);

    const BOOL sent = WinHttpSendRequest(
        request, L"Content-Type: application/json\r\n", static_cast<DWORD>(-1),
        const_cast<void*>(static_cast<const void*>(body.data())), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                        WINHTTP_NO_HEADER_INDEX);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return status_code >= 200 && status_code < 300;
}

}  // namespace

EventForwarder::EventForwarder(UploadSettings settings) : settings_(std::move(settings)) {}

void EventForwarder::forward(const AuditEvent& event) {
    if (!enabled()) {
        return;
    }

    const std::string body = build_ingest_dto_json(event);
    const std::string url = settings_.ingest_url;
    std::thread([url, body]() { (void)post_json(url, body); }).detach();
}

}  // namespace useraudit
