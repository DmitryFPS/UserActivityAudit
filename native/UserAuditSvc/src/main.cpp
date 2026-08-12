#include "useraudit/event_loop.hpp"
#include "useraudit/service_host.hpp"

#include <iostream>

namespace {

constexpr wchar_t kServiceName[] = L"UserAuditSvc";
constexpr wchar_t kServiceDisplayName[] = L"User Activity Audit Service";

void print_usage() {
    std::wcerr << L"Usage:\n"
               << L"  UserAuditSvc.exe              Run service (or dev console mode)\n"
               << L"  UserAuditSvc.exe --install    Install Windows service (admin)\n"
               << L"  UserAuditSvc.exe --uninstall  Remove Windows service (admin)\n";
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc >= 2) {
        const std::wstring arg = argv[1];
        if (arg == L"--install") {
            if (!useraudit::install_service(kServiceName, kServiceDisplayName,
                                            useraudit::get_module_path().c_str())) {
                return 1;
            }
            std::wcout << L"Service installed: " << kServiceName << L"\n";
            return 0;
        }
        if (arg == L"--uninstall") {
            if (!useraudit::uninstall_service(kServiceName)) {
                return 1;
            }
            std::wcout << L"Service removed: " << kServiceName << L"\n";
            return 0;
        }
        print_usage();
        return 1;
    }

    volatile bool stop_requested = false;

    const int rc = useraudit::run_service(
        kServiceName,
        [&stop_requested]() {
            useraudit::run_event_loop(stop_requested);
        },
        [&stop_requested]() { stop_requested = true; });

    return rc;
}
