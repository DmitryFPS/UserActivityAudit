#include "useraudit/session_collector.hpp"

#include "useraudit/time_utils.hpp"

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

namespace useraudit {

namespace {

constexpr wchar_t kSecurityChannel[] = L"Security";
constexpr wchar_t kSessionQuery[] =
    L"*[System[(EventID=4624 or EventID=4634 or EventID=4647 or EventID=4800 or EventID=4801)]]";

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
    const std::wstring pattern_sq = L"Name='" + std::wstring(name) + L"'>";
    const size_t pos = xml.find(pattern_sq);
    if (pos != std::wstring_view::npos) {
        const size_t value_start = pos + pattern_sq.size();
        const size_t end = xml.find(L"</Data>", value_start);
        if (end != std::wstring_view::npos) {
            return xml.substr(value_start, end - value_start);
        }
    }

    const std::wstring pattern_dq = L"Name=\"" + std::wstring(name) + L"\">";
    const size_t pos2 = xml.find(pattern_dq);
    if (pos2 == std::wstring_view::npos) {
        return {};
    }
    const size_t value_start = pos2 + pattern_dq.size();
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

std::string map_session_action(int event_id) {
    switch (event_id) {
        case 4624:
            return "login";
        case 4634:
        case 4647:
            return "logout";
        case 4800:
            return "lock";
        case 4801:
            return "unlock";
        default:
            return "unknown";
    }
}

}  // namespace

SessionCollector::SessionCollector(EventSink& writer, std::string hostname)
    : writer_(writer), hostname_(std::move(hostname)) {}

SessionCollector::~SessionCollector() {
    stop();
}

DWORD WINAPI SessionCollector::notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context,
                                               EVT_HANDLE event_record) {
    auto* self = static_cast<SessionCollector*>(context);
    if (self == nullptr) {
        return 0;
    }

    if (self->stop_flag_ != nullptr && *self->stop_flag_) {
        return 0;
    }

    if (action == EvtSubscribeActionError) {
        OutputDebugStringW(L"[UserAuditSvc] SessionCollector subscription error\n");
        return 0;
    }

    if (action == EvtSubscribeActionDeliver && event_record != nullptr) {
        self->handle_event(event_record);
    }

    return 0;
}

bool SessionCollector::handle_event(EVT_HANDLE event_record) {
    DWORD buffer_used = 0;
    DWORD property_count = 0;
    if (!EvtRender(nullptr, event_record, EvtRenderEventXml, 0, nullptr, &buffer_used,
                   &property_count)) {
        const DWORD err = GetLastError();
        if (err != ERROR_INSUFFICIENT_BUFFER) {
            return false;
        }
    }

    std::vector<wchar_t> buffer(buffer_used / sizeof(wchar_t) + 1);
    if (!EvtRender(nullptr, event_record, EvtRenderEventXml, buffer_used, buffer.data(),
                   &buffer_used, &property_count)) {
        return false;
    }

    const std::wstring xml(buffer.data());
    const std::wstring_view event_id_view = extract_xml_value(xml, L"EventID");
    if (event_id_view.empty()) {
        return false;
    }

    const int event_id = _wtoi(std::wstring(event_id_view).c_str());
    const std::string action = map_session_action(event_id);
    if (action == "unknown") {
        return false;
    }

    AuditEvent event;
    event.id = generate_event_id();
    event.ts = iso8601_from_event_xml(xml);
    event.lvl = 1;
    event.cat = "session";
    event.act = action;
    event.sev = "info";
    event.host = hostname_;
    event.src = "eventlog";

    const std::string domain = wide_to_utf8(extract_data_value(xml, L"TargetDomainName"));
    const std::string username = wide_to_utf8(extract_data_value(xml, L"TargetUserName"));
    const std::string subject_domain = wide_to_utf8(extract_data_value(xml, L"SubjectDomainName"));
    const std::string subject_user = wide_to_utf8(extract_data_value(xml, L"SubjectUserName"));

    if (event_id == 4800 || event_id == 4801 || event_id == 4634 || event_id == 4647) {
        if (!subject_domain.empty() && !subject_user.empty()) {
            event.user = subject_domain + "\\" + subject_user;
        } else if (!subject_user.empty()) {
            event.user = subject_user;
        }
    } else if (!domain.empty() && !username.empty()) {
        event.user = domain + "\\" + username;
    } else if (!username.empty()) {
        event.user = username;
    }

    if (event_id == 4624) {
        const std::string logon_type = wide_to_utf8(extract_data_value(xml, L"LogonType"));
        const std::string ip = wide_to_utf8(extract_data_value(xml, L"IpAddress"));
        if (!logon_type.empty()) {
            event.data["logon_type"] = logon_type;
        }
        if (!ip.empty() && ip != "-") {
            event.data["ip_address"] = ip;
        }
    }

    event.data["event_id"] = std::to_string(event_id);

    const bool written = writer_.write(event);
    if (written && session_observer_) {
        session_observer_(action, event.user);
    }
    return written;
}

bool SessionCollector::start() {
    if (subscription_ != nullptr) {
        return true;
    }

    subscription_ = EvtSubscribe(nullptr, nullptr, kSecurityChannel, kSessionQuery, nullptr, this,
                                 SessionCollector::notify_callback, EvtSubscribeToFutureEvents);

    if (subscription_ == nullptr) {
        OutputDebugStringW(L"[UserAuditSvc] EvtSubscribe Security channel failed\n");
        return false;
    }

    return true;
}

void SessionCollector::stop() {
    if (subscription_ != nullptr) {
        EvtClose(subscription_);
        subscription_ = nullptr;
    }
}

}  // namespace useraudit
