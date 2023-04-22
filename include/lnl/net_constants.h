#pragma once

#include <cstdint>
#include <array>

namespace lnl {
    struct net_constants final {
        net_constants() = delete;

        ~net_constants() = delete;

        static constexpr int32_t DEFAULT_WINDOW_SIZE = 64;
        static constexpr int32_t SOCKET_BUFFER_SIZE = 1024 * 1024; //1mb
        static constexpr int32_t SOCKET_TTL = 255;

        static constexpr int32_t HEADER_SIZE = 1;
        static constexpr int32_t CHANNELED_HEADER_SIZE = 4;
        static constexpr int32_t CONNECT_REQUEST_HEADER_SIZE = 18;
        static constexpr int32_t CONNECT_ACCEPT_HEADER_SIZE = 15;
        static constexpr int32_t FRAGMENT_HEADER_SIZE = 6;
        static constexpr int32_t FRAGMENTED_HEADER_TOTAL_SIZE = CHANNELED_HEADER_SIZE + FRAGMENT_HEADER_SIZE;
        static constexpr uint16_t MAX_SEQUENCE = 32768;
        static constexpr uint16_t HALF_MAX_SEQUENCE = MAX_SEQUENCE / 2;

        static constexpr int32_t PROTOCOL_ID = 13;
        static constexpr int32_t MAX_UDP_HEADER_SIZE = 68;
        static constexpr int32_t CHANNEL_TYPE_COUNT = 4;

        static constexpr std::array<int32_t, 7> POSSIBLE_MTU{
                576 - MAX_UDP_HEADER_SIZE,  //minimal (RFC 1191)
                1024,                       //most games standard
                1232 - MAX_UDP_HEADER_SIZE,
                1460 - MAX_UDP_HEADER_SIZE, //google cloud
                1472 - MAX_UDP_HEADER_SIZE, //VPN
                1492 - MAX_UDP_HEADER_SIZE, //Ethernet with LLC and SNAP, PPPoE (RFC 1042)
                1500 - MAX_UDP_HEADER_SIZE  //Ethernet II (RFC 1191)
        };

        static constexpr int32_t MAX_PACKET_SIZE = POSSIBLE_MTU.back();
        static constexpr int32_t MAX_UNRELIABLE_DATA_SIZE = MAX_PACKET_SIZE - HEADER_SIZE;

        static constexpr uint8_t MAX_CONNECTION_NUMBER = 4;
    };
}