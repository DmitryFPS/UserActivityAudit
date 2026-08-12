#include "useraudit/usb_collector.hpp"

#include "useraudit/time_utils.hpp"
#include "useraudit/usb_helpers.hpp"

#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>

#include <string>

#pragma comment(lib, "wbemuuid.lib")

namespace useraudit {

namespace {

constexpr int kVolumeArrival = 2;
constexpr int kVolumeRemoval = 3;

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::wstring variant_to_wstring(const VARIANT& value) {
    if (value.vt == VT_BSTR && value.bstrVal != nullptr) {
        return value.bstrVal;
    }
    if (value.vt == VT_I2) {
        return std::to_wstring(value.iVal);
    }
    if (value.vt == VT_I4) {
        return std::to_wstring(value.lVal);
    }
    if (value.vt == VT_UI2) {
        return std::to_wstring(value.uiVal);
    }
    if (value.vt == VT_UI4) {
        return std::to_wstring(value.ulVal);
    }
    return {};
}

int variant_to_int(const VARIANT& value) {
    if (value.vt == VT_I2) {
        return value.iVal;
    }
    if (value.vt == VT_I4) {
        return value.lVal;
    }
    if (value.vt == VT_UI2) {
        return value.uiVal;
    }
    if (value.vt == VT_UI4) {
        return static_cast<int>(value.ulVal);
    }
    return 0;
}

bool get_wmi_property(IWbemClassObject* object, const wchar_t* name, VARIANT& out) {
    VariantInit(&out);
    if (object == nullptr) {
        return false;
    }
    return SUCCEEDED(object->Get(name, 0, &out, nullptr, nullptr));
}

bool query_pnp_device_id_for_drive(IWbemServices* services, const std::wstring& drive_name,
                                   std::wstring& pnp_device_id, std::wstring& interface_type) {
    if (services == nullptr || drive_name.empty()) {
        return false;
    }

    std::wstring query =
        L"ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='" + drive_name +
        L"'} WHERE AssocClass=Win32_LogicalDiskToPartition ResultClass=Win32_DiskPartition";

    IEnumWbemClassObject* partition_enum = nullptr;
    BSTR wql = SysAllocString(L"WQL");
    BSTR query_bstr = SysAllocString(query.c_str());
    const HRESULT partition_hr =
        services->ExecQuery(wql, query_bstr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            nullptr, &partition_enum);
    SysFreeString(query_bstr);

    if (FAILED(partition_hr) || partition_enum == nullptr) {
        SysFreeString(wql);
        return false;
    }

    IWbemClassObject* partition = nullptr;
    ULONG returned = 0;
    bool found = false;

    if (SUCCEEDED(partition_enum->Next(WBEM_INFINITE, 1, &partition, &returned)) && returned > 0 &&
        partition != nullptr) {
        VARIANT device_id{};
        if (get_wmi_property(partition, L"DeviceID", device_id)) {
            const std::wstring partition_id = variant_to_wstring(device_id);
            VariantClear(&device_id);

            if (!partition_id.empty()) {
                std::wstring drive_query =
                    L"ASSOCIATORS OF {Win32_DiskPartition.DeviceID='" + partition_id +
                    L"'} WHERE ResultClass=Win32_DiskDrive";

                IEnumWbemClassObject* drive_enum = nullptr;
                BSTR drive_query_bstr = SysAllocString(drive_query.c_str());
                const HRESULT drive_hr = services->ExecQuery(
                    wql, drive_query_bstr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    nullptr, &drive_enum);
                SysFreeString(drive_query_bstr);

                if (SUCCEEDED(drive_hr) && drive_enum != nullptr) {
                    IWbemClassObject* disk = nullptr;
                    ULONG drive_returned = 0;
                    if (SUCCEEDED(drive_enum->Next(WBEM_INFINITE, 1, &disk, &drive_returned)) &&
                        drive_returned > 0 && disk != nullptr) {
                        VARIANT pnp{};
                        VARIANT iface{};
                        if (get_wmi_property(disk, L"PNPDeviceID", pnp)) {
                            pnp_device_id = variant_to_wstring(pnp);
                            VariantClear(&pnp);
                        }
                        if (get_wmi_property(disk, L"InterfaceType", iface)) {
                            interface_type = variant_to_wstring(iface);
                            VariantClear(&iface);
                        }
                        found = !pnp_device_id.empty();
                        disk->Release();
                    }
                    drive_enum->Release();
                }
            }
        }
        partition->Release();
    }

    partition_enum->Release();
    SysFreeString(wql);
    return found;
}

}  // namespace

UsbCollector::UsbCollector(EventSink& writer, std::string hostname)
    : writer_(writer), hostname_(std::move(hostname)) {}

UsbCollector::~UsbCollector() {
    stop();
}

void UsbCollector::wmi_thread_main() {
    HRESULT com_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_initialized = SUCCEEDED(com_hr) || com_hr == RPC_E_CHANGED_MODE;
    if (!com_initialized) {
        running_.store(false);
        return;
    }

    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_NONE,
                       RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    IWbemLocator* locator = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                reinterpret_cast<LPVOID*>(&locator))) ||
        locator == nullptr) {
        CoUninitialize();
        running_.store(false);
        return;
    }

    IWbemServices* services = nullptr;
    BSTR namespace_path = SysAllocString(L"ROOT\\CIMV2");
    if (FAILED(locator->ConnectServer(namespace_path, nullptr, nullptr, 0, 0, 0, 0, &services)) ||
        services == nullptr) {
        SysFreeString(namespace_path);
        locator->Release();
        CoUninitialize();
        running_.store(false);
        return;
    }

    CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    BSTR wql = SysAllocString(L"WQL");
    BSTR query = SysAllocString(
        L"SELECT * FROM Win32_VolumeChangeEvent WHERE EventType = 2 OR EventType = 3");
    IEnumWbemClassObject* enumerator = nullptr;
    if (FAILED(services->ExecNotificationQuery(wql, query, WBEM_FLAG_SEND_STATUS, nullptr,
                                               &enumerator)) ||
        enumerator == nullptr) {
        SysFreeString(query);
        SysFreeString(wql);
        SysFreeString(namespace_path);
        services->Release();
        locator->Release();
        CoUninitialize();
        running_.store(false);
        return;
    }

    SysFreeString(query);
    SysFreeString(wql);

    while (running_.load()) {
        if (stop_flag_ != nullptr && *stop_flag_) {
            break;
        }

        IWbemClassObject* event_object = nullptr;
        ULONG returned = 0;
        const HRESULT next_hr = enumerator->Next(1000, 1, &event_object, &returned);
        if (next_hr == WBEM_S_TIMEDOUT || returned == 0) {
            continue;
        }
        if (FAILED(next_hr) || event_object == nullptr) {
            break;
        }

        VARIANT event_type_value{};
        VARIANT drive_name_value{};
        int event_type = 0;
        std::wstring drive_name;

        if (get_wmi_property(event_object, L"EventType", event_type_value)) {
            event_type = variant_to_int(event_type_value);
            VariantClear(&event_type_value);
        }
        if (get_wmi_property(event_object, L"DriveName", drive_name_value)) {
            drive_name = variant_to_wstring(drive_name_value);
            VariantClear(&drive_name_value);
        }

        if (!drive_name.empty() && (event_type == kVolumeArrival || event_type == kVolumeRemoval)) {
            const std::wstring root = drive_name.size() == 2 && drive_name[1] == L':'
                                          ? drive_name + L"\\"
                                          : drive_name;

            if (GetDriveTypeW(root.c_str()) == DRIVE_REMOVABLE) {
                AuditEvent event;
                event.id = generate_event_id();
                event.ts = utc_now_iso8601();
                event.lvl = 1;
                event.cat = "usb";
                event.act = (event_type == kVolumeArrival) ? "insert" : "remove";
                event.sev = "info";
                event.host = hostname_;
                event.src = "wmi";
                event.data["drive"] = wide_to_utf8(drive_name);

                if (event_type == kVolumeArrival) {
                    wchar_t volume_name[MAX_PATH + 1] = {};
                    DWORD serial = 0;
                    if (GetVolumeInformationW(root.c_str(), volume_name, MAX_PATH, &serial, nullptr,
                                            nullptr, nullptr, 0)) {
                        if (volume_name[0] != L'\0') {
                            event.data["volume_label"] = wide_to_utf8(volume_name);
                        }
                        if (serial != 0) {
                            event.data["volume_serial"] = std::to_string(serial);
                        }
                    }

                    std::wstring pnp_device_id;
                    std::wstring interface_type;
                    if (query_pnp_device_id_for_drive(services, drive_name, pnp_device_id,
                                                      interface_type)) {
                        if (!interface_type.empty()) {
                            event.data["interface_type"] = wide_to_utf8(interface_type);
                        }

                        UsbVolumeIdentity identity;
                        if (parse_usb_vid_pid(wide_to_utf8(pnp_device_id), identity)) {
                            event.data["vid"] = identity.vid;
                            event.data["pid"] = identity.pid;
                            if (!identity.serial.empty()) {
                                event.data["serial"] = identity.serial;
                            }
                            event.data["pnp_device_id"] = identity.pnp_device_id;
                        }
                    }
                }

                if (event_type == kVolumeArrival && correlation_ != nullptr) {
                    event.corr = correlation_->on_usb_insert(wide_to_utf8(drive_name));
                } else if (event_type == kVolumeRemoval && correlation_ != nullptr) {
                    correlation_->on_usb_remove(wide_to_utf8(drive_name));
                }

                writer_.write(event);
            }
        }

        event_object->Release();
    }

    enumerator->Release();
    SysFreeString(namespace_path);
    services->Release();
    locator->Release();
    CoUninitialize();
    running_.store(false);
}

bool UsbCollector::start() {
    if (running_.load()) {
        return true;
    }

    running_.store(true);
    wmi_thread_ = std::thread([this]() { wmi_thread_main(); });
    return true;
}

void UsbCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (wmi_thread_.joinable()) {
        wmi_thread_.join();
    }
}

}  // namespace useraudit
