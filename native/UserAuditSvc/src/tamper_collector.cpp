#include "useraudit/tamper_collector.hpp"

#include "useraudit/lockdown_manager.hpp"
#include "useraudit/paths.hpp"
#include "useraudit/time_utils.hpp"

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

namespace useraudit {

namespace {

constexpr wchar_t kSystemChannel[] = L"System";
constexpr wchar_t kSecurityChannel[] = L"Security";
constexpr wchar_t kServiceQuery[] =
    L"*[System[Provider[@Name='Service Control Manager'] and (EventID=7036 or EventID=7040)]]";
constexpr wchar_t kSecurityQuery[] = L"*[System[(EventID=4663)]]";

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

std::wstring read_event_xml(EVT_HANDLE event_record) {
    DWORD buffer_used = 0;
    DWORD property_count = 0;
    if (!EvtRender(nullptr, event_record, EvtRenderEventXml, 0, nullptr, &buffer_used,
                   &property_count) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return {};
    }

    std::vector<wchar_t> buffer((buffer_used / sizeof(wchar_t)) + 1, L'\0');
    if (!EvtRender(nullptr, event_record, EvtRenderEventXml,
                   static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(),
                   &buffer_used, &property_count)) {
        return {};
    }
    return std::wstring(buffer.data());
}

}  // namespace

TamperCollector::TamperCollector(EventSink& writer, std::string hostname)
    : writer_(writer), hostname_(std::move(hostname)) {}

TamperCollector::~TamperCollector() {
    stop();
}

void TamperCollector::emit_tamper(const std::string& act, const std::string& detail) {
    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 2;
    event.cat = "tamper";
    event.act = act;
    event.sev = "critical";
    event.host = hostname_;
    event.src = "eventlog";
    event.data["detail"] = detail;
    writer_.write(event);

    if (lockdown_manager_ != nullptr) {
        lockdown_manager_->activate(detail);
    }
}

bool TamperCollector::handle_system_event(EVT_HANDLE event_record) {
    const std::wstring xml = read_event_xml(event_record);
    if (xml.empty()) {
        return false;
    }

    const std::wstring_view message = extract_xml_value(xml, L"Message");
    if (message.find(L"UserAuditSvc") == std::wstring_view::npos) {
        return false;
    }

    const std::wstring_view event_id = extract_xml_value(xml, L"EventID");
    const std::string detail = wide_to_utf8(message);
    if (event_id == L"7036" && message.find(L"stopped") != std::wstring_view::npos) {
        emit_tamper("service_stop", detail);
        return true;
    }
    if (event_id == L"7040") {
        emit_tamper("service_config_change", detail);
        return true;
    }
    return false;
}

bool TamperCollector::handle_security_event(EVT_HANDLE event_record) {
    const std::wstring xml = read_event_xml(event_record);
    if (xml.empty()) {
        return false;
    }

    const std::wstring data_root = resolve_data_root().wstring();
    const std::wstring logs_root = resolve_log_directory().wstring();
    if (xml.find(data_root) == std::wstring::npos && xml.find(logs_root) == std::wstring::npos &&
        xml.find(L"UserAudit") == std::wstring::npos) {
        return false;
    }

    const std::wstring_view access = extract_xml_value(xml, L"AccessMask");
    if (access.empty()) {
        return false;
    }

    emit_tamper("attempt_denied", wide_to_utf8(extract_xml_value(xml, L"ObjectName")));
    return true;
}

DWORD WINAPI TamperCollector::system_notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                                     PVOID context, EVT_HANDLE event_record) {
    if (action != EvtSubscribeActionDeliver || context == nullptr || event_record == nullptr) {
        return 0;
    }

    auto* self = static_cast<TamperCollector*>(context);
    if (self->stop_flag_ != nullptr && *self->stop_flag_) {
        return 0;
    }

    self->handle_system_event(event_record);
    return 0;
}

DWORD WINAPI TamperCollector::security_notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                                       PVOID context, EVT_HANDLE event_record) {
    if (action != EvtSubscribeActionDeliver || context == nullptr || event_record == nullptr) {
        return 0;
    }

    auto* self = static_cast<TamperCollector*>(context);
    if (self->stop_flag_ != nullptr && *self->stop_flag_) {
        return 0;
    }

    self->handle_security_event(event_record);
    return 0;
}

bool TamperCollector::start() {
    system_subscription_ =
        EvtSubscribe(nullptr, nullptr, kSystemChannel, kServiceQuery, nullptr, this,
                   system_notify_callback, EvtSubscribeToFutureEvents);
    if (system_subscription_ == nullptr) {
        OutputDebugStringW(L"[UserAuditSvc] TamperCollector System subscription failed\n");
    }

    security_subscription_ =
        EvtSubscribe(nullptr, nullptr, kSecurityChannel, kSecurityQuery, nullptr, this,
                     security_notify_callback, EvtSubscribeToFutureEvents);
    if (security_subscription_ == nullptr) {
        OutputDebugStringW(L"[UserAuditSvc] TamperCollector Security subscription failed\n");
    }

    return system_subscription_ != nullptr || security_subscription_ != nullptr;
}

void TamperCollector::stop() {
    if (system_subscription_ != nullptr) {
        EvtClose(system_subscription_);
        system_subscription_ = nullptr;
    }
    if (security_subscription_ != nullptr) {
        EvtClose(security_subscription_);
        security_subscription_ = nullptr;
    }
}

}  // namespace useraudit
