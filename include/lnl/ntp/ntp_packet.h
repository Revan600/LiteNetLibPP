#pragma once

#include <cstdint>
#include <vector>

#include <lnl/net_time.h>

namespace lnl {
    class ntp_packet final {
        static const net_time EPOCH;

        std::vector<uint8_t> m_data;
    public:
        [[nodiscard]] NTP_LEAP_INDICATOR leap_indicator() const {
            return (NTP_LEAP_INDICATOR) ((m_data[0] & 0xC0) >> 6);
        }

        [[nodiscard]] int32_t version_number() const {
            return (m_data[0] & 0x38) >> 3;
        }

        [[nodiscard]] NTP_MODE mode() const {
            return (NTP_MODE) (m_data[0] & 0x07);
        }

        [[nodiscard]] int32_t stratum() const {
            return m_data[1];
        }

        [[nodiscard]] int32_t poll() const {
            return m_data[2];
        }

        [[nodiscard]] int32_t precision() const {
            return (int8_t) m_data[2];
        }

        [[nodiscard]] int32_t root_delay() const {
            
        }

    private:
        void set_version_number(int32_t value) {
            m_data[0] = (uint8_t) ((m_data[0] & ~0x38) | value << 3);
        }

        void set_ntp_mode(NTP_MODE value) {
            m_data[0] = (uint8_t) ((m_data[0] & ~0x07) | (int) value);
        }
    };
}