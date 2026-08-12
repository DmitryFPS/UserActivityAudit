#include "useraudit/event_loop.hpp"
#include "useraudit/service_host.hpp"
#include "useraudit/user_agent_mode.hpp"

#include <iostream>
#include <string>

namespace {

constexpr wchar_t kServiceName[] = L"UserAuditSvc";
constexpr wchar_t kServiceDisplayName[] = L"User Activity Audit Service";

bool has_argument(int argc, wchar_t** argv, const wchar_t* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::wstring(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

void print_usage() {
    std::wcerr << L"Usage:\n"
               << L"  UserAudit.exe              Run audit service (or dev console mode)\n"
               << L"  UserAudit.exe --install    Install Windows service (admin)\n"
               << L"  UserAudit.exe --uninstall  Remove Windows service (admin)\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (has_argument(argc, argv, L"--user-agent")) {
        return useraudit::run_user_agent_mode(argc, argv);
    }

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
        [&stop_requested]() { useraudit::run_event_loop(stop_requested); },
        [&stop_requested]() { stop_requested = true; });

    return rc;
}
