#include "useraudit/print_collector.hpp"

#include "useraudit/time_utils.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace useraudit {

namespace {

constexpr wchar_t kPrintChannel[] = L"Microsoft-Windows-PrintService/Operational";
constexpr wchar_t kPrintQuery[] = L"*[System[(EventID=307)]]";

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

}  // namespace

PrintCollector::PrintCollector(EventSink& writer, std::string hostname)
    : writer_(writer), hostname_(std::move(hostname)) {}

PrintCollector::~PrintCollector() {
    stop();
}

DWORD WINAPI PrintCollector::notify_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context,
                                             EVT_HANDLE event_record) {
    if (action != EvtSubscribeActionDeliver || context == nullptr || event_record == nullptr) {
        return ERROR_SUCCESS;
    }

    auto* collector = static_cast<PrintCollector*>(context);
    if (collector->stop_flag_ != nullptr && *collector->stop_flag_) {
        return ERROR_SUCCESS;
    }

    collector->handle_event(event_record);
    return ERROR_SUCCESS;
}

bool PrintCollector::handle_event(EVT_HANDLE event_record) {
    DWORD buffer_used = 0;
    DWORD property_count = 0;
    if (!EvtRender(nullptr, event_record, EvtRenderEventXml, 0, nullptr, &buffer_used,
                   &property_count) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return false;
    }

    std::vector<wchar_t> buffer(buffer_used / sizeof(wchar_t) + 1);
    if (!EvtRender(nullptr, event_record, EvtRenderEventXml, buffer_used, buffer.data(),
                   &buffer_used, &property_count)) {
        return false;
    }

    const std::wstring_view xml(buffer.data(), buffer_used / sizeof(wchar_t));
    const std::wstring_view event_id = extract_xml_value(xml, L"EventID");
    if (event_id.empty()) {
        return false;
    }

    AuditEvent event;
    event.id = generate_event_id();
    event.ts = iso8601_from_event_xml(xml);
    if (event.ts.empty()) {
        event.ts = utc_now_iso8601();
    }
    event.lvl = 2;
    event.cat = "print";
    event.act = "job";
    event.sev = "info";
    event.host = hostname_;
    event.src = "eventlog";
    event.data["event_id"] = wide_to_utf8(event_id);

    const std::wstring_view printer = extract_xml_value(xml, L"Computer");
    if (!printer.empty()) {
        event.data["printer_host"] = wide_to_utf8(printer);
    }

    return writer_.write(event);
}

bool PrintCollector::start() {
    if (subscription_ != nullptr) {
        return true;
    }

    subscription_ = EvtSubscribe(
        nullptr, nullptr, kPrintChannel, kPrintQuery, nullptr, this,
        reinterpret_cast<EVT_SUBSCRIBE_CALLBACK>(notify_callback), EvtSubscribeToFutureEvents);
    return subscription_ != nullptr;
}

void PrintCollector::stop() {
    if (subscription_ != nullptr) {
        EvtClose(subscription_);
        subscription_ = nullptr;
    }
}

}  // namespace useraudit
