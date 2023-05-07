#pragma once

#include <lnl/net_address.h>

#include <cstdint>
#include <lnl/net_manager.h>

namespace lnl {
    class ntp_request final {
        static constexpr int32_t RESEND_TIMER = 1000;
        static constexpr int32_t KILL_TIMER = 10000;

        int32_t m_resend_time = RESEND_TIMER;
        int32_t m_kill_time = KILL_TIMER;

        const net_address m_ntp_endpoint;
    public:
        static constexpr int32_t DEFAULT_PORT = 123;

        ntp_request(const net_address& endpoint) : m_ntp_endpoint(endpoint) {}

        bool need_to_kill() const {
            return m_kill_time >= KILL_TIMER;
        }

        bool send(SOCKET socket, int32_t time) {
            m_resend_time += time;
            m_kill_time += time;

            if (m_resend_time < RESEND_TIMER) {
                return false;
            }

            
        }
    };
}