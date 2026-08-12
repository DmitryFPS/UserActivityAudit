#include "useraudit/clipboard_collector.hpp"

#include "useraudit/time_utils.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace useraudit {

namespace {

std::string sha256_hex(const std::string& input) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return {};
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    if (!BCRYPT_SUCCESS(BCryptHashData(
            hash, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
            static_cast<ULONG>(input.size()), 0))) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::uint8_t digest[32]{};
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0))) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (const auto byte : digest) {
        out.push_back(kHex[(byte >> 4) & 0x0F]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

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

}  // namespace

ClipboardCollector::ClipboardCollector(EventSink& writer, std::string hostname,
                                       int poll_interval_sec)
    : writer_(writer),
      hostname_(std::move(hostname)),
      poll_interval_sec_(poll_interval_sec > 0 ? poll_interval_sec : 10) {}

ClipboardCollector::~ClipboardCollector() {
    stop();
}

bool ClipboardCollector::emit_if_changed() {
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data == nullptr) {
        CloseClipboard();
        return false;
    }

    const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
    if (text == nullptr) {
        CloseClipboard();
        return false;
    }

    const std::string utf8 = wide_to_utf8(text);
    GlobalUnlock(data);
    CloseClipboard();

    if (utf8.empty()) {
        return false;
    }

    const std::string hash = sha256_hex(utf8);
    if (hash.empty() || hash == last_hash_) {
        return false;
    }

    last_hash_ = hash;

    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 2;
    event.cat = "clipboard";
    event.act = "hash";
    event.sev = "info";
    event.host = hostname_;
    event.src = "user32";
    event.data["sha256"] = hash;
    event.data["length"] = std::to_string(utf8.size());
    return writer_.write(event);
}

void ClipboardCollector::poll_thread_main() {
    while (running_.load()) {
        if (stop_flag_ != nullptr && *stop_flag_) {
            break;
        }

        emit_if_changed();

        for (int i = 0; i < poll_interval_sec_ * 10; ++i) {
            if (!running_.load() || (stop_flag_ != nullptr && *stop_flag_)) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool ClipboardCollector::start() {
    if (running_.load()) {
        return true;
    }

    running_.store(true);
    poll_thread_ = std::thread([this]() { poll_thread_main(); });
    return true;
}

void ClipboardCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
}

}  // namespace useraudit
