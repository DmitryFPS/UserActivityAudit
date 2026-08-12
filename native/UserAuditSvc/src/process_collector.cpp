#include "useraudit/process_collector.hpp"

#include "useraudit/audit_event.hpp"
#include "useraudit/time_utils.hpp"

#include <windows.h>
#include <evntcons.h>
#include <evntrace.h>
#include <tdh.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace useraudit {

namespace {

// Microsoft-Windows-Kernel-Process
constexpr GUID kKernelProcessProvider = {
    0x22FB2CD6, 0x0E7B, 0x422B, {0xA0, 0xC7, 0x2F, 0xAD, 0x1F, 0xD0, 0xE7, 0x16}};

constexpr wchar_t kSessionName[] = L"UserAuditKernelProcess";

struct TraceContext {
    ProcessCollector* collector = nullptr;
    EventSink* writer = nullptr;
    std::string hostname;
};

TraceContext g_trace_context{};

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

std::wstring query_process_path(unsigned long pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return {};
    }

    std::wstring path;
    path.resize(MAX_PATH);
    DWORD size = static_cast<DWORD>(path.size());
    if (QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
        path.resize(size);
    } else {
        path.clear();
    }
    CloseHandle(process);
    return path;
}

bool get_uint32_property(PEVENT_RECORD record, const wchar_t* property_name, unsigned long& out) {
    PROPERTY_DATA_DESCRIPTOR descriptor{};
    descriptor.PropertyName = reinterpret_cast<ULONGLONG>(property_name);
    descriptor.ArrayIndex = ULONG_MAX;

    DWORD property_size = 0;
    if (TdhGetPropertySize(record, 0, nullptr, 1, &descriptor, &property_size) != ERROR_SUCCESS ||
        property_size < sizeof(DWORD)) {
        return false;
    }

    DWORD value = 0;
    if (TdhGetProperty(record, 0, nullptr, 1, &descriptor, property_size,
                       reinterpret_cast<PBYTE>(&value)) != ERROR_SUCCESS) {
        return false;
    }

    out = value;
    return true;
}

bool get_unicode_string_property(PEVENT_RECORD record, const wchar_t* property_name,
                                 std::wstring& out) {
    PROPERTY_DATA_DESCRIPTOR descriptor{};
    descriptor.PropertyName = reinterpret_cast<ULONGLONG>(property_name);
    descriptor.ArrayIndex = ULONG_MAX;

    DWORD property_size = 0;
    if (TdhGetPropertySize(record, 0, nullptr, 1, &descriptor, &property_size) != ERROR_SUCCESS ||
        property_size == 0) {
        return false;
    }

    std::vector<BYTE> buffer(property_size);
    if (TdhGetProperty(record, 0, nullptr, 1, &descriptor, property_size, buffer.data()) !=
        ERROR_SUCCESS) {
        return false;
    }

    if (property_size < sizeof(WCHAR)) {
        return false;
    }

    const auto* chars = reinterpret_cast<const wchar_t*>(buffer.data());
    const size_t char_count = property_size / sizeof(wchar_t);
    size_t length = char_count;
    if (length > 0 && chars[length - 1] == L'\0') {
        --length;
    }
    out.assign(chars, length);
    return true;
}

void emit_process_event(EventSink& writer, const std::string& hostname,
                        const std::string& action, unsigned long pid, unsigned long parent_pid,
                        const std::wstring& image_name, const std::wstring& image_path) {
    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 1;
    event.cat = "process";
    event.act = action;
    event.sev = "info";
    event.host = hostname;
    event.src = "etw";

    event.data["pid"] = std::to_string(pid);
    if (parent_pid != 0) {
        event.data["parent_pid"] = std::to_string(parent_pid);
    }

    if (!image_path.empty()) {
        event.data["path"] = wide_to_utf8(image_path);
    } else if (!image_name.empty()) {
        event.data["image_name"] = wide_to_utf8(image_name);
    }

    writer.write(event);
}

void WINAPI process_event_callback(PEVENT_RECORD record) {
    if (record == nullptr || g_trace_context.writer == nullptr) {
        return;
    }

    if (!IsEqualGUID(record->EventHeader.ProviderId, kKernelProcessProvider)) {
        return;
    }

    const UCHAR opcode = record->EventHeader.EventDescriptor.Opcode;
    if (opcode != 1 && opcode != 2) {
        return;
    }

    unsigned long pid = 0;
    unsigned long parent_pid = 0;
    std::wstring image_name;

    if (!get_uint32_property(record, L"ProcessId", pid)) {
        pid = record->EventHeader.ProcessId;
    }
    get_uint32_property(record, L"ParentProcessId", parent_pid);
    get_unicode_string_property(record, L"ImageFileName", image_name);

    const std::wstring image_path = query_process_path(pid);
    const std::string action = (opcode == 1) ? "start" : "stop";

    emit_process_event(*g_trace_context.writer, g_trace_context.hostname, action, pid, parent_pid,
                       image_name, image_path);
}

}  // namespace

ProcessCollector::ProcessCollector(EventSink& writer, std::string hostname)
    : writer_(writer), hostname_(std::move(hostname)) {}

ProcessCollector::~ProcessCollector() {
    stop();
}

void ProcessCollector::trace_thread_main() {
    const size_t buffer_size = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(kSessionName);
    auto buffer = std::make_unique<BYTE[]>(buffer_size);
    auto* properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.get());

    ZeroMemory(properties, sizeof(EVENT_TRACE_PROPERTIES));
    properties->Wnode.BufferSize = static_cast<ULONG>(buffer_size);
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.ClientContext = 1;  // QPC timer
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    CONTROLTRACE_ID session_id{};
    if (StartTraceW(&session_id, kSessionName, properties) != ERROR_SUCCESS) {
        OutputDebugStringW(L"[UserAuditSvc] ProcessCollector StartTrace failed\n");
        running_.store(false);
        return;
    }
    session_id_value_ = session_id;

    if (EnableTraceEx2(static_cast<TRACEHANDLE>(session_id), &kKernelProcessProvider,
                      EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, 0, 0, 0,
                      nullptr) != ERROR_SUCCESS) {
        OutputDebugStringW(L"[UserAuditSvc] ProcessCollector EnableTraceEx2 failed\n");
        ControlTraceW(session_id, kSessionName, properties, EVENT_TRACE_CONTROL_STOP);
        session_id_value_ = 0;
        running_.store(false);
        return;
    }

    EVENT_TRACE_LOGFILEW logfile{};
    logfile.LoggerName = const_cast<LPWSTR>(kSessionName);
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = process_event_callback;

    trace_handle_ = OpenTraceW(&logfile);
    if (trace_handle_ == INVALID_PROCESSTRACE_HANDLE) {
        OutputDebugStringW(L"[UserAuditSvc] ProcessCollector OpenTrace failed\n");
        ControlTraceW(session_id, kSessionName, properties, EVENT_TRACE_CONTROL_STOP);
        session_id_value_ = 0;
        running_.store(false);
        return;
    }

    ProcessTrace(&trace_handle_, 1, nullptr, nullptr);

    CloseTrace(trace_handle_);
    trace_handle_ = ~0ULL;

    ZeroMemory(properties, sizeof(EVENT_TRACE_PROPERTIES));
    properties->Wnode.BufferSize = static_cast<ULONG>(buffer_size);
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    ControlTraceW(session_id, kSessionName, properties, EVENT_TRACE_CONTROL_STOP);
    session_id_value_ = 0;
    running_.store(false);
}

bool ProcessCollector::start() {
    if (running_.load()) {
        return true;
    }

    g_trace_context.collector = this;
    g_trace_context.writer = &writer_;
    g_trace_context.hostname = hostname_;

    running_.store(true);
    trace_thread_ = std::thread([this]() { trace_thread_main(); });
    return true;
}

void ProcessCollector::stop() {
    if (!running_.load()) {
        return;
    }

    if (trace_handle_ != ~0ULL) {
        CloseTrace(trace_handle_);
        trace_handle_ = ~0ULL;
    }

    if (session_id_value_ != 0) {
        const size_t buffer_size = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(kSessionName);
        auto buffer = std::make_unique<BYTE[]>(buffer_size);
        auto* properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.get());
        ZeroMemory(properties, sizeof(EVENT_TRACE_PROPERTIES));
        properties->Wnode.BufferSize = static_cast<ULONG>(buffer_size);
        properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        CONTROLTRACE_ID session_id = session_id_value_;
        ControlTraceW(session_id, kSessionName, properties, EVENT_TRACE_CONTROL_STOP);
        session_id_value_ = 0;
    }

    if (trace_thread_.joinable()) {
        trace_thread_.join();
    }

    running_.store(false);
}

}  // namespace useraudit
