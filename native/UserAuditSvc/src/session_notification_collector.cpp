#include "useraudit/session_notification_collector.hpp"

#include "useraudit/time_utils.hpp"

#include <windows.h>
#include <wtsapi32.h>

#include <string>

namespace useraudit {

namespace {

constexpr wchar_t kWindowClassName[] = L"UserAuditSessionNotify";
constexpr WPARAM kWtsSessionLock = 0x7;
constexpr WPARAM kWtsSessionUnlock = 0x8;

SessionNotificationCollector* collector_from_hwnd(HWND hwnd) {
    return reinterpret_cast<SessionNotificationCollector*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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

LRESULT CALLBACK session_notify_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_WTSSESSION_CHANGE) {
        auto* self = collector_from_hwnd(hwnd);
        if (self != nullptr) {
            self->on_wts_session_change(wparam);
        }
        return 0;
    }

    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

SessionNotificationCollector::SessionNotificationCollector(EventSink& sink, std::string hostname,
                                                             unsigned long session_id)
    : sink_(sink), hostname_(std::move(hostname)), session_id_(session_id) {}

SessionNotificationCollector::~SessionNotificationCollector() {
    stop();
}

void SessionNotificationCollector::set_stop_flag(volatile bool* flag) {
    stop_flag_ = flag;
}

std::string SessionNotificationCollector::resolve_session_user() const {
    LPWSTR user_buffer = nullptr;
    DWORD bytes = 0;
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id_, WTSUserName,
                                     &user_buffer, &bytes)) {
        return {};
    }

    std::string user = wide_to_utf8(user_buffer != nullptr ? user_buffer : L"");
    WTSFreeMemory(user_buffer);

    LPWSTR domain_buffer = nullptr;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id_, WTSDomainName,
                                    &domain_buffer, &bytes)) {
        const std::string domain =
            wide_to_utf8(domain_buffer != nullptr ? domain_buffer : L"");
        WTSFreeMemory(domain_buffer);
        if (!domain.empty() && !user.empty()) {
            return domain + "\\" + user;
        }
    }

    return user;
}

void SessionNotificationCollector::emit_session_event(const std::string& action) {
    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 1;
    event.cat = "session";
    event.act = action;
    event.sev = "info";
    event.host = hostname_;
    event.src = "wts";
    event.sess = static_cast<int>(session_id_);
    event.user = resolve_session_user();
    event.data["session_id"] = std::to_string(session_id_);

    sink_.write(event);
}

void SessionNotificationCollector::on_wts_session_change(WPARAM event_type) {
    switch (event_type) {
        case kWtsSessionLock:
            emit_session_event("lock");
            break;
        case kWtsSessionUnlock:
            emit_session_event("unlock");
            break;
        default:
            break;
    }
}

void SessionNotificationCollector::message_thread_main() {
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = session_notify_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClassName;
    RegisterClassW(&window_class);  // OK if class already registered (ERROR_CLASS_ALREADY_EXISTS)

    HWND hwnd = CreateWindowExW(0, kWindowClassName, L"UserAuditSessionNotify", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        OutputDebugStringW(L"[UserAudit] SessionNotificationCollector CreateWindow failed\n");
        running_.store(false);
        return;
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    window_handle_ = hwnd;

    if (!WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION)) {
        OutputDebugStringW(L"[UserAudit] WTSRegisterSessionNotification failed\n");
        DestroyWindow(hwnd);
        window_handle_ = nullptr;
        running_.store(false);
        return;
    }

    MSG message{};
    while (running_.load()) {
        if (stop_flag_ != nullptr && *stop_flag_) {
            break;
        }

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running_.store(false);
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (!running_.load()) {
            break;
        }

        WaitMessage();
    }

    WTSUnRegisterSessionNotification(hwnd);
    DestroyWindow(hwnd);
    window_handle_ = nullptr;
    running_.store(false);
}

bool SessionNotificationCollector::start() {
    if (running_.load()) {
        return true;
    }

    running_.store(true);
    message_thread_ = std::thread([this]() { message_thread_main(); });
    return true;
}

void SessionNotificationCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (window_handle_ != nullptr) {
        PostMessageW(window_handle_, WM_QUIT, 0, 0);
    }

    if (message_thread_.joinable()) {
        message_thread_.join();
    }
}

}  // namespace useraudit
