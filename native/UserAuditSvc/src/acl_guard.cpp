#include "useraudit/acl_guard.hpp"

#include "useraudit/paths.hpp"
#include "useraudit/time_utils.hpp"

#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace useraudit {

namespace {

constexpr DWORD kVerifyIntervalSec = 300;

bool create_well_known_sid(WELL_KNOWN_SID_TYPE type, PSID& sid_out, std::vector<BYTE>& storage) {
    DWORD sid_size = SECURITY_MAX_SID_SIZE;
    storage.assign(sid_size, 0);
    sid_out = storage.data();
    if (!CreateWellKnownSid(type, nullptr, sid_out, &sid_size)) {
        sid_out = nullptr;
        return false;
    }
    return true;
}

bool acl_denies_users_write(PACL acl) {
    if (acl == nullptr) {
        return false;
    }

    std::vector<BYTE> users_storage;
    PSID users_sid = nullptr;
    if (!create_well_known_sid(WinBuiltinUsersSid, users_sid, users_storage)) {
        return false;
    }

    for (WORD i = 0; i < acl->AceCount; ++i) {
        LPVOID ace = nullptr;
        if (GetAce(acl, i, &ace) != TRUE || ace == nullptr) {
            continue;
        }

        const auto* header = static_cast<ACE_HEADER*>(ace);
        if (header->AceType != ACCESS_DENIED_ACE_TYPE) {
            continue;
        }

        const auto* denied = static_cast<ACCESS_DENIED_ACE*>(ace);
        const PSID ace_sid = reinterpret_cast<PSID>(const_cast<PVOID>(static_cast<const void*>(&denied->SidStart)));
        if (!EqualSid(ace_sid, users_sid)) {
            continue;
        }

        const ACCESS_MASK mask = denied->Mask;
        if ((mask & (DELETE | FILE_WRITE_DATA | FILE_APPEND_DATA | WRITE_DAC | WRITE_OWNER)) != 0) {
            return true;
        }
    }

    return false;
}

}  // namespace

std::wstring expected_users_deny_sddl_fragment() {
    return L"(D;;WD;;;BU)";
}

AclGuard::AclGuard(EventSink& writer, std::string hostname)
    : writer_(writer), hostname_(std::move(hostname)) {}

AclGuard::~AclGuard() {
    stop();
}

bool AclGuard::apply_to_path(const std::filesystem::path& path, bool is_directory) const {
    std::vector<BYTE> system_storage;
    std::vector<BYTE> admin_storage;
    std::vector<BYTE> users_storage;
    PSID system_sid = nullptr;
    PSID admin_sid = nullptr;
    PSID users_sid = nullptr;

    if (!create_well_known_sid(WinLocalSystemSid, system_sid, system_storage) ||
        !create_well_known_sid(WinBuiltinAdministratorsSid, admin_sid, admin_storage) ||
        !create_well_known_sid(WinBuiltinUsersSid, users_sid, users_storage)) {
        return false;
    }

    EXPLICIT_ACCESSW entries[3]{};
    entries[0].grfAccessPermissions = GENERIC_ALL;
    entries[0].grfAccessMode = SET_ACCESS;
    entries[0].grfInheritance =
        is_directory ? (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE) : NO_INHERITANCE;
    entries[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entries[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    entries[0].Trustee.ptstrName = static_cast<LPWSTR>(system_sid);

    entries[1].grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE | FILE_WRITE_DATA;
    entries[1].grfAccessMode = SET_ACCESS;
    entries[1].grfInheritance =
        is_directory ? (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE) : NO_INHERITANCE;
    entries[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entries[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    entries[1].Trustee.ptstrName = static_cast<LPWSTR>(admin_sid);

    entries[2].grfAccessPermissions =
        DELETE | FILE_WRITE_DATA | FILE_APPEND_DATA | WRITE_DAC | WRITE_OWNER;
    entries[2].grfAccessMode = DENY_ACCESS;
    entries[2].grfInheritance =
        is_directory ? (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE) : NO_INHERITANCE;
    entries[2].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entries[2].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    entries[2].Trustee.ptstrName = static_cast<LPWSTR>(users_sid);

    PACL acl = nullptr;
    if (SetEntriesInAclW(3, entries, nullptr, &acl) != ERROR_SUCCESS || acl == nullptr) {
        return false;
    }

    const std::wstring native = path.wstring();
    const DWORD result = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(native.c_str()), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, system_sid, nullptr, acl, nullptr);
    LocalFree(acl);
    return result == ERROR_SUCCESS;
}

bool AclGuard::apply() {
    const auto root = resolve_data_root();
    const auto logs = resolve_log_directory();
    const auto keys = resolve_keys_directory();
    const auto config = resolve_config_path();

    std::error_code ec;
    std::filesystem::create_directories(logs, ec);
    std::filesystem::create_directories(keys, ec);
    std::filesystem::create_directories(root, ec);

    bool ok = apply_to_path(root, true);
    ok = apply_to_path(logs, true) && ok;
    ok = apply_to_path(keys, true) && ok;

    if (std::filesystem::exists(config)) {
        ok = apply_to_path(config, false) && ok;
    }

    const auto chain_state = resolve_chain_state_path();
    if (std::filesystem::exists(chain_state)) {
        ok = apply_to_path(chain_state, false) && ok;
    }

    return ok;
}

bool AclGuard::verify() const {
    const auto logs = resolve_log_directory();
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PACL acl = nullptr;
    PACL sacl = nullptr;

    const DWORD result = GetNamedSecurityInfoW(
        logs.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, &sacl,
        &descriptor);
    if (result != ERROR_SUCCESS || descriptor == nullptr) {
        if (descriptor != nullptr) {
            LocalFree(descriptor);
        }
        return false;
    }

    const bool ok = acl_denies_users_write(acl);

    LocalFree(descriptor);
    return ok;
}

void AclGuard::emit_tamper_event(const std::string& reason) const {
    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 2;
    event.cat = "tamper";
    event.act = "attempt_denied";
    event.sev = "critical";
    event.host = hostname_;
    event.src = "acl_guard";
    event.data["reason"] = reason;
    writer_.write(event);
}

void AclGuard::guard_thread_main() {
    while (running_.load() && (stop_flag_ == nullptr || !*stop_flag_)) {
        if (!verify()) {
            OutputDebugStringW(L"[UserAuditSvc] AclGuard verify failed — reapplying ACL\n");
            if (!apply()) {
                emit_tamper_event("acl_reapply_failed");
            }
        }

        for (int i = 0; i < kVerifyIntervalSec; ++i) {
            if (!running_.load() || (stop_flag_ != nullptr && *stop_flag_)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    running_.store(false);
}

bool AclGuard::start() {
    if (running_.load()) {
        return true;
    }
    if (!apply()) {
        OutputDebugStringW(L"[UserAuditSvc] AclGuard initial apply failed\n");
    }
    running_.store(true);
    guard_thread_ = std::thread([this]() { guard_thread_main(); });
    return true;
}

void AclGuard::stop() {
    running_.store(false);
    if (guard_thread_.joinable()) {
        guard_thread_.join();
    }
}

}  // namespace useraudit
