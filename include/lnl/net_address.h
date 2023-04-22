#pragma once

#include <WinSock2.h>
#include <string>
#include <WS2tcpip.h>

namespace lnl {
    struct net_address final {
        struct sockaddr_in raw{};

        net_address() {
            raw.sin_family = AF_INET;
        }

        explicit net_address(struct sockaddr_in& addr) : raw(addr) {}

        const std::string& address() const {
            if (!m_cached_address.empty()) {
                return m_cached_address;
            }

            m_cached_address.resize(INET_ADDRSTRLEN + 1, '\0');
            inet_ntop(raw.sin_family, &raw.sin_addr, m_cached_address.data(), m_cached_address.size());
            m_cached_address.erase(m_cached_address.find_last_not_of('\0') + 1, std::string::npos);
            return m_cached_address;
        }

        [[nodiscard]] uint16_t port() const {
            return ntohs(raw.sin_port);
        }

        [[nodiscard]] std::string to_string() const {
            return address() + ":" + std::to_string(port());
        }

        bool operator==(const net_address& other) const {
            return other.raw.sin_addr.S_un.S_addr == raw.sin_addr.S_un.S_addr &&
                   other.raw.sin_port == raw.sin_port &&
                   other.raw.sin_family == raw.sin_family;
        }

    private:
        mutable std::string m_cached_address;
    };

    struct net_address_hash final {
        size_t operator()(const net_address& key) const {
            return (uint64_t) key.raw.sin_addr.S_un.S_addr << 32 | key.raw.sin_port; //simply join ip and port
        }
    };
}