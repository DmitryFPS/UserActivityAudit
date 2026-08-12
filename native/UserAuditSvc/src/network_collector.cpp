#include "useraudit/network_collector.hpp"

#include "useraudit/time_utils.hpp"

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

namespace useraudit {

namespace {

std::string ipv4_to_string(unsigned long addr) {
    in_addr value{};
    value.S_un.S_addr = addr;
    char buffer[INET_ADDRSTRLEN]{};
    if (InetNtopA(AF_INET, &value, buffer, sizeof(buffer)) == nullptr) {
        return {};
    }
    return buffer;
}

unsigned short extract_port(unsigned long port_dw) {
    return ntohs(static_cast<u_short>((port_dw >> 8) & 0xFF00) | static_cast<u_short>(port_dw & 0xFF));
}

}  // namespace

NetworkCollector::NetworkCollector(EventSink& writer, std::string hostname,
                                   const ConfigManager& config)
    : writer_(writer), hostname_(std::move(hostname)), config_(config) {}

NetworkCollector::~NetworkCollector() {
    stop();
}

void NetworkCollector::emit_snapshot() {
    ULONG buffer_size = 0;
    if (GetExtendedTcpTable(nullptr, &buffer_size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) !=
            ERROR_INSUFFICIENT_BUFFER &&
        buffer_size == 0) {
        return;
    }

    std::vector<std::uint8_t> buffer(buffer_size);
    auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    const DWORD result =
        GetExtendedTcpTable(table, &buffer_size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (result != NO_ERROR || table == nullptr) {
        return;
    }

    AuditEvent event;
    event.id = generate_event_id();
    event.ts = utc_now_iso8601();
    event.lvl = 2;
    event.cat = "network";
    event.act = "snapshot";
    event.sev = "info";
    event.host = hostname_;
    event.src = "iphlpapi";

    std::size_t emitted = 0;
    for (DWORD i = 0; i < table->dwNumEntries && emitted < 64; ++i) {
        const auto& row = table->table[i];
        if (row.dwState != MIB_TCP_STATE_ESTAB) {
            continue;
        }

        const std::string remote_ip = ipv4_to_string(row.dwRemoteAddr);
        if (remote_ip.empty() || remote_ip == "0.0.0.0") {
            continue;
        }

        const unsigned short remote_port = extract_port(row.dwRemotePort);
        const unsigned short local_port = extract_port(row.dwLocalPort);

        std::ostringstream remote;
        remote << remote_ip << ':' << remote_port;

        const std::string index = std::to_string(emitted);
        event.data["pid_" + index] = std::to_string(row.dwOwningPid);
        event.data["remote_" + index] = remote.str();
        event.data["local_" + index] = std::to_string(local_port);
        ++emitted;
    }

    if (emitted == 0) {
        return;
    }

    event.data["count"] = std::to_string(emitted);
    writer_.write(event);
}

void NetworkCollector::poll_thread_main() {
    while (running_.load()) {
        if (stop_flag_ != nullptr && *stop_flag_) {
            break;
        }

        emit_snapshot();

        const int poll_sec = config_.network_poll_sec();
        for (int tick = 0; tick < poll_sec * 10; ++tick) {
            if (!running_.load() || (stop_flag_ != nullptr && *stop_flag_)) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool NetworkCollector::start() {
    if (running_.load()) {
        return true;
    }

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }

    running_.store(true);
    poll_thread_ = std::thread([this]() { poll_thread_main(); });
    return true;
}

void NetworkCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    WSACleanup();
}

}  // namespace useraudit
