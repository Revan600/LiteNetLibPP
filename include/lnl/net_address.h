#pragma once

#include <string>
#include <cstring>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#elif __linux__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#endif

namespace lnl {
    struct net_address final {
        struct sockaddr_in raw{};

        net_address() {
            raw.sin_family = AF_INET;
        }

        explicit net_address(struct sockaddr_in& addr) : raw(addr) {}

        net_address(const std::string& address, uint16_t port) {
            set_address(address);
            set_port(port);
        }

        void set_address(const std::string& address) {
            addrinfo* result;
            auto oldPort = port();

            if (getaddrinfo(address.c_str(), nullptr, nullptr, &result) != 0) {
                //todo: log somehow?
                return;
            }

            for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
                if (ptr->ai_family != AF_INET) {
                    continue;
                }

                raw = *(decltype(raw)*) ptr->ai_addr;

                break;
            }

            freeaddrinfo(result);

            set_port(oldPort);
        }

        const std::string& address() const {
            if (!m_cached_address.empty() && compare_and_update_addr()) {
                return m_cached_address;
            }

            m_cached_address.resize(INET_ADDRSTRLEN + 1, '\0');
            inet_ntop(raw.sin_family, &raw.sin_addr, m_cached_address.data(), m_cached_address.size());
            m_cached_address.erase(m_cached_address.find_last_not_of('\0') + 1, std::string::npos);
            m_cached_string_representation = {};
            return m_cached_address;
        }

        void set_port(uint16_t port) {
            raw.sin_port = htons(port);
        }

        [[nodiscard]] uint16_t port() const {
            return ntohs(raw.sin_port);
        }

        [[nodiscard]] const std::string& to_string() const {
            if (!m_cached_string_representation.empty() && compare_and_update_addr()) {
                return m_cached_string_representation;
            }
            m_cached_address = {};
            m_cached_string_representation = address() + ":" + std::to_string(port());
            return m_cached_string_representation;
        }

        bool operator==(const net_address& other) const {
#ifdef WIN32
            return other.raw.sin_addr.S_un.S_addr == raw.sin_addr.S_un.S_addr &&
                   other.raw.sin_port == raw.sin_port &&
                   other.raw.sin_family == raw.sin_family;
#elif __linux__
            return other.raw.sin_addr.s_addr == raw.sin_addr.s_addr &&
                   other.raw.sin_port == raw.sin_port &&
                   other.raw.sin_family == raw.sin_family;
#endif
        }

    private:
        bool compare_and_update_addr() const {
            auto result = memcmp(&raw, &m_old_addr, sizeof(raw)) == 0;

            if (!result) {
                m_old_addr = raw;
            }

            return result;
        }

        mutable std::string m_cached_address;
        mutable std::string m_cached_string_representation;
        mutable decltype(raw) m_old_addr{};
    };

    struct net_address_hash final {
        size_t operator()(const net_address& key) const {
#ifdef WIN32
            return (uint64_t) key.raw.sin_addr.S_un.S_addr << 32 | key.raw.sin_port; //simply join ip and port
#elif __linux__
            return (uint64_t) key.raw.sin_addr.s_addr << 32 | key.raw.sin_port; //simply join ip and port
#endif
        }
    };
}