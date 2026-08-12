#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace useraudit {

// Runs fn as a Windows service named service_name, or in dev console mode when
// USERAUDIT_DEV_CONSOLE is defined and the process is not launched by SCM.
int run_service(const wchar_t* service_name, const std::function<void()>& on_start,
                const std::function<void()>& on_stop);

// Install/remove helpers for development (Phase 9 moves to MSI).
bool install_service(const wchar_t* service_name, const wchar_t* display_name,
                     const wchar_t* binary_path);
bool uninstall_service(const wchar_t* service_name);

std::wstring get_module_path();

}  // namespace useraudit
