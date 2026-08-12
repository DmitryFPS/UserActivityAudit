#pragma once

#include "useraudit/driver_ioctl.hpp"

#include <atomic>

namespace useraudit {

class DriverClient {
public:
    DriverClient();
    ~DriverClient();

    DriverClient(const DriverClient&) = delete;
    DriverClient& operator=(const DriverClient&) = delete;

    [[nodiscard]] bool is_connected() const { return connected_; }
    bool connect();
    void disconnect();

    bool set_lockdown(bool enabled);
    bool query_status(UserAuditFilterStatus& status);

private:
    void* port_handle_ = nullptr;
    bool connected_ = false;
};

DriverClient& global_driver_client();

}  // namespace useraudit
