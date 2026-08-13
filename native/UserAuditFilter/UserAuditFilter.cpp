#include <fltKernel.h>

#include "useraudit/driver_ioctl.hpp"

#pragma prefast(disable : __WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

static PFLT_FILTER g_filter_handle = nullptr;
static PFLT_PORT g_server_port = nullptr;
static PSECURITY_DESCRIPTOR g_security_descriptor = nullptr;
static UNICODE_STRING g_port_name = RTL_CONSTANT_STRING(L"\\UserAuditFilter");
static volatile LONG g_lockdown_active = 0;
static volatile LONG g_protected_hits = 0;

static BOOLEAN UserAuditIsProtectedPath(_In_ PFLT_CALLBACK_DATA Data) {
    PFLT_FILE_NAME_INFORMATION name_info = nullptr;
    if (!NT_SUCCESS(FltGetFileNameInformation(
            Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &name_info))) {
        return FALSE;
    }

    FltParseFileNameInformation(name_info);
    const UNICODE_STRING marker = RTL_CONSTANT_STRING(L"\\UserAudit\\");
    const BOOLEAN protected_path = name_info->Name.Buffer != nullptr &&
                                   wcsstr(name_info->Name.Buffer, marker.Buffer) != nullptr;
    FltReleaseFileNameInformation(name_info);
    return protected_path;
}

static FLT_PREOP_CALLBACK_STATUS UserAuditPreSetInformation(
    _Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (!UserAuditIsProtectedPath(Data)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    const FILE_INFORMATION_CLASS info_class =
        Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
    if (info_class == FileDispositionInformation || info_class == FileDispositionInformationEx ||
        info_class == FileRenameInformation || info_class == FileRenameInformationEx) {
        InterlockedIncrement(&g_protected_hits);
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;
        return FLT_PREOP_COMPLETE;
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

static FLT_PREOP_CALLBACK_STATUS UserAuditPreWrite(_Inout_ PFLT_CALLBACK_DATA Data,
                                                   _In_ PCFLT_RELATED_OBJECTS FltObjects,
                                                   _Flt_CompletionContext_Outptr_ PVOID* CompletionContext) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (!g_lockdown_active || !UserAuditIsProtectedPath(Data)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    InterlockedIncrement(&g_protected_hits);
    Data->IoStatus.Status = STATUS_ACCESS_DENIED;
    Data->IoStatus.Information = 0;
    return FLT_PREOP_COMPLETE;
}

static NTSTATUS UserAuditPortConnect(_In_ PFLT_PORT ClientPort, _In_opt_ PVOID ServerPortCookie,
                                     _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
                                     _In_ ULONG SizeOfContext,
                                     _Outptr_result_maybenull_ PVOID* ConnectionPortCookie) {
    UNREFERENCED_PARAMETER(ClientPort);
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionPortCookie);
    return STATUS_SUCCESS;
}

static VOID UserAuditPortDisconnect(_In_opt_ PVOID ConnectionCookie) {
    UNREFERENCED_PARAMETER(ConnectionCookie);
}

static NTSTATUS UserAuditPortMessage(_In_ PVOID PortCookie,
                                     _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
                                     _In_ ULONG InputBufferLength,
                                     _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength)
                                         PVOID OutputBuffer,
                                     _In_ ULONG OutputBufferLength, _Out_ PULONG ReturnOutputBufferLength) {
    UNREFERENCED_PARAMETER(PortCookie);

    if (InputBuffer == nullptr || InputBufferLength < sizeof(ULONG)) {
        return STATUS_INVALID_PARAMETER;
    }

    const ULONG command = *static_cast<PULONG>(InputBuffer);
    if (command == USERAUDIT_IOCTL_SET_LOCKDOWN) {
        if (InputBufferLength < sizeof(ULONG) * 2) {
            return STATUS_INVALID_PARAMETER;
        }
        const ULONG value = *(static_cast<PULONG>(InputBuffer) + 1);
        InterlockedExchange(&g_lockdown_active, value ? 1 : 0);
        return STATUS_SUCCESS;
    }

    if (command == USERAUDIT_IOCTL_QUERY_STATUS) {
        if (OutputBuffer == nullptr || OutputBufferLength < sizeof(USERAUDIT_FILTER_STATUS)) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        auto* status = static_cast<PUSERAUDIT_FILTER_STATUS>(OutputBuffer);
        status->driver_loaded = 1;
        status->lockdown_active = static_cast<ULONG>(g_lockdown_active);
        status->protected_hits = static_cast<ULONG>(g_protected_hits);
        *ReturnOutputBufferLength = sizeof(USERAUDIT_FILTER_STATUS);
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS UserAuditUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags) {
    UNREFERENCED_PARAMETER(Flags);
    if (g_server_port != nullptr) {
        FltCloseCommunicationPort(g_server_port);
        g_server_port = nullptr;
    }
    if (g_filter_handle != nullptr) {
        FltUnregisterFilter(g_filter_handle);
        g_filter_handle = nullptr;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS UserAuditInstanceSetup(_In_ PCFLT_RELATED_OBJECTS FltObjects,
                                       _In_ FLT_INSTANCE_SETUP_FLAGS Flags, _In_ DEVICE_TYPE VolumeDeviceType,
                                       _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);
    return STATUS_SUCCESS;
}

static CONST FLT_OPERATION_REGISTRATION UserAuditCallbacks[] = {
    {IRP_MJ_SET_INFORMATION, 0, UserAuditPreSetInformation, nullptr},
    {IRP_MJ_WRITE, 0, UserAuditPreWrite, nullptr},
    {IRP_MJ_OPERATION_END}};

static CONST FLT_REGISTRATION UserAuditRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    nullptr,
    UserAuditCallbacks,
    UserAuditUnload,
    UserAuditInstanceSetup,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = FltRegisterFilter(DriverObject, &UserAuditRegistration, &g_filter_handle);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FltBuildDefaultSecurityDescriptor(&g_security_descriptor, FLT_PORT_ALL_ACCESS);
    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(g_filter_handle);
        return status;
    }

    OBJECT_ATTRIBUTES object_attributes;
    InitializeObjectAttributes(&object_attributes, &g_port_name, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               nullptr, g_security_descriptor);

    status = FltCreateCommunicationPort(g_filter_handle, &g_server_port, &object_attributes, nullptr,
                                      UserAuditPortConnect, UserAuditPortDisconnect, UserAuditPortMessage, 1);
    FltFreeSecurityDescriptor(g_security_descriptor);
    g_security_descriptor = nullptr;

    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(g_filter_handle);
        return status;
    }

    status = FltStartFiltering(g_filter_handle);
    if (!NT_SUCCESS(status)) {
        FltCloseCommunicationPort(g_server_port);
        FltUnregisterFilter(g_filter_handle);
    }
    return status;
}
