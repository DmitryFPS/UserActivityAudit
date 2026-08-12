#include "useraudit/service_host.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>

namespace useraudit {

namespace {

SERVICE_STATUS g_service_status{};
SERVICE_STATUS_HANDLE g_status_handle = nullptr;
std::atomic<bool> g_stop_requested{false};
const wchar_t* g_service_name = nullptr;
std::function<void()> g_on_start;
std::function<void()> g_on_stop;
std::thread g_worker;

void report_status(DWORD state, DWORD win32_exit_code = NO_ERROR, DWORD wait_hint = 0) {
    g_service_status.dwCurrentState = state;
    g_service_status.dwWin32ExitCode = win32_exit_code;
    g_service_status.dwWaitHint = wait_hint;
    g_service_status.dwControlsAccepted =
        (state == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP;
    SetServiceStatus(g_status_handle, &g_service_status);
}

void WINAPI service_control_handler(DWORD control) {
    switch (control) {
        case SERVICE_CONTROL_STOP:
            report_status(SERVICE_STOP_PENDING, NO_ERROR, 3000);
            g_stop_requested.store(true);
            if (g_on_stop) {
                g_on_stop();
            }
            break;
        default:
            break;
    }
}

void WINAPI service_main(DWORD /*argc*/, LPWSTR* /*argv*/) {
    g_status_handle = RegisterServiceCtrlHandlerW(g_service_name, service_control_handler);
    if (!g_status_handle) {
        return;
    }

    g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_service_status.dwControlsAccepted = 0;
    report_status(SERVICE_START_PENDING, NO_ERROR, 3000);

    g_stop_requested.store(false);
    g_worker = std::thread([]() {
        if (g_on_start) {
            g_on_start();
        }
    });

    report_status(SERVICE_RUNNING);

    if (g_worker.joinable()) {
        g_worker.join();
    }

    report_status(SERVICE_STOPPED);
}

void run_console_dev_mode(const std::function<void()>& on_start,
                          const std::function<void()>& on_stop) {
    std::wcout << L"[UserAuditSvc] Development console mode. Press Ctrl+C to stop.\n";
    g_stop_requested.store(false);

    std::thread worker([&on_start]() {
        if (on_start) {
            on_start();
        }
    });

    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (on_stop) {
        on_stop();
    }

    if (worker.joinable()) {
        worker.join();
    }
}

}  // namespace

int run_service(const wchar_t* service_name, const std::function<void()>& on_start,
                const std::function<void()>& on_stop) {
    g_service_name = service_name;
    g_on_start = on_start;
    g_on_stop = on_stop;

    SERVICE_TABLE_ENTRYW dispatch_table[] = {
        {const_cast<LPWSTR>(service_name), service_main},
        {nullptr, nullptr},
    };

#if defined(USERAUDIT_DEV_CONSOLE)
    if (!StartServiceCtrlDispatcherW(dispatch_table)) {
        const DWORD err = GetLastError();
        if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            run_console_dev_mode(on_start, on_stop);
            return 0;
        }
        std::wcerr << L"StartServiceCtrlDispatcher failed: " << err << L"\n";
        return static_cast<int>(err);
    }
    return 0;
#else
    if (!StartServiceCtrlDispatcherW(dispatch_table)) {
        const DWORD err = GetLastError();
        std::wcerr << L"StartServiceCtrlDispatcher failed: " << err << L"\n";
        return static_cast<int>(err);
    }
    return 0;
#endif
}

std::wstring get_module_path() {
    wchar_t path[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return L"";
    }
    return std::wstring(path, len);
}

bool install_service(const wchar_t* service_name, const wchar_t* display_name,
                     const wchar_t* binary_path) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        return false;
    }

    SC_HANDLE service = CreateServiceW(
        scm, service_name, display_name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, binary_path, nullptr, nullptr, nullptr,
        nullptr, nullptr);

    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_DESCRIPTIONW desc{};
    desc.lpDescription = const_cast<LPWSTR>(
        L"Commercial user activity audit agent (UserActivityAudit).");
    ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &desc);

    SC_ACTION actions[3] = {
        {SC_ACTION_RESTART, 60000},
        {SC_ACTION_RESTART, 60000},
        {SC_ACTION_NONE, 0},
    };
    SERVICE_FAILURE_ACTIONSW failure{};
    failure.dwResetPeriod = 86400;
    failure.cActions = 3;
    failure.lpsaActions = actions;
    ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return true;
}

bool uninstall_service(const wchar_t* service_name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        return false;
    }

    SC_HANDLE service =
        OpenServiceW(scm, service_name, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status{};
    ControlService(service, SERVICE_CONTROL_STOP, &status);
    const bool deleted = DeleteService(service);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return deleted;
}

}  // namespace useraudit
