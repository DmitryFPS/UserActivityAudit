#include "useraudit/driver_client.hpp"

#include <windows.h>
#include <fltUser.h>

namespace useraudit {

namespace {

DriverClient g_driver_client;

}  // namespace

DriverClient& global_driver_client() {
    return g_driver_client;
}

DriverClient::DriverClient() = default;

DriverClient::~DriverClient() {
    disconnect();
}

bool DriverClient::connect() {
    if (connected_) {
        return true;
    }

    HANDLE port = nullptr;
    const HRESULT hr =
        FilterConnectCommunicationPort(kFilterPortName, 0, nullptr, 0, nullptr, &port);
    if (FAILED(hr) || port == nullptr) {
        return false;
    }

    port_handle_ = port;
    connected_ = true;
    return true;
}

void DriverClient::disconnect() {
    if (port_handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(port_handle_));
        port_handle_ = nullptr;
    }
    connected_ = false;
}

bool DriverClient::set_lockdown(bool enabled) {
    if (!connected_ && !connect()) {
        return false;
    }

    ULONG message[2] = {useraudit::kIoctlUserAuditSetLockdown, enabled ? 1UL : 0UL};
    DWORD bytes_returned = 0;
    const HRESULT hr = FilterSendMessage(static_cast<HANDLE>(port_handle_), message, sizeof(message),
                                         nullptr, 0, &bytes_returned);
    UNREFERENCED_PARAMETER(bytes_returned);
    return SUCCEEDED(hr);
}

bool DriverClient::query_status(UserAuditFilterStatus& status) {
    if (!connected_ && !connect()) {
        return false;
    }

    ULONG command = useraudit::kIoctlUserAuditQueryStatus;
    DWORD bytes_returned = 0;
    const HRESULT hr =
        FilterSendMessage(static_cast<HANDLE>(port_handle_), &command, sizeof(command), &status,
                          sizeof(status), &bytes_returned);
    return SUCCEEDED(hr) && bytes_returned == sizeof(status);
}

}  // namespace useraudit
