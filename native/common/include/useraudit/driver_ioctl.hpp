#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>

typedef struct _USERAUDIT_FILTER_STATUS {
    ULONG driver_loaded;
    ULONG lockdown_active;
    ULONG protected_hits;
} USERAUDIT_FILTER_STATUS, *PUSERAUDIT_FILTER_STATUS;

#else
#include <windows.h>
#include <winioctl.h>

#include <cstdint>

namespace useraudit {

struct UserAuditFilterStatus {
    std::uint32_t driver_loaded = 1;
    std::uint32_t lockdown_active = 0;
    std::uint32_t protected_hits = 0;
};

inline constexpr wchar_t kFilterPortName[] = L"\\UserAuditFilter";

#endif

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif

#define USERAUDIT_IOCTL_SET_LOCKDOWN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define USERAUDIT_IOCTL_QUERY_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define USERAUDIT_IOCTL_SUBMIT_AUTH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifndef _KERNEL_MODE
inline constexpr DWORD kIoctlUserAuditSetLockdown = USERAUDIT_IOCTL_SET_LOCKDOWN;
inline constexpr DWORD kIoctlUserAuditQueryStatus = USERAUDIT_IOCTL_QUERY_STATUS;
inline constexpr DWORD kIoctlUserAuditSubmitAuth = USERAUDIT_IOCTL_SUBMIT_AUTH;
}  // namespace useraudit
#endif
