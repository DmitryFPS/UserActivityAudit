#include <windows.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {

constexpr wchar_t kServiceName[] = L"UserAuditSvc";
constexpr DWORD kPollIntervalMs = 30000;

std::atomic<bool> g_stop_requested{false};

BOOL WINAPI console_handler(DWORD ctrl) {
    if (ctrl == CTRL_C_EVENT || ctrl == CTRL_BREAK_EVENT || ctrl == CTRL_CLOSE_EVENT) {
        g_stop_requested.store(true);
        return TRUE;
    }
    return FALSE;
}

bool is_service_running() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        return false;
    }

    SC_HANDLE service = OpenServiceW(scm, kServiceName, SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytes_needed = 0;
    const BOOL ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                         reinterpret_cast<LPBYTE>(&status), sizeof(status),
                                         &bytes_needed);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    if (!ok) {
        return false;
    }
    return status.dwCurrentState == SERVICE_RUNNING ||
           status.dwCurrentState == SERVICE_START_PENDING;
}

bool start_service() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        return false;
    }

    SC_HANDLE service = OpenServiceW(scm, kServiceName, SERVICE_START | SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    const BOOL started = StartServiceW(service, 0, nullptr);
    const DWORD last_error = started ? ERROR_SUCCESS : GetLastError();
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return started || last_error == ERROR_SERVICE_ALREADY_RUNNING;
}

}  // namespace

int wmain() {
    SetConsoleCtrlHandler(console_handler, TRUE);
    std::wcout << L"[UserAuditWatchdog] Monitoring " << kServiceName << L" every 30s. Ctrl+C to stop.\n";

    while (!g_stop_requested.load()) {
        if (!is_service_running()) {
            std::wcout << L"[UserAuditWatchdog] Service not running — restarting...\n";
            if (start_service()) {
                std::wcout << L"[UserAuditWatchdog] StartService requested.\n";
            } else {
                std::wcerr << L"[UserAuditWatchdog] StartService failed: " << GetLastError() << L"\n";
            }
        }

        for (DWORD elapsed = 0; elapsed < kPollIntervalMs && !g_stop_requested.load();
             elapsed += 500) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    std::wcout << L"[UserAuditWatchdog] Stopped.\n";
    return 0;
}
