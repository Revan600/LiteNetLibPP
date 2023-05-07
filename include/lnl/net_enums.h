#pragma once

#include <cstdint>

namespace lnl {
    enum class NET_EVENT_TYPE {
        CONNECT,
        DISCONNECT,
        RECEIVE,
        RECEIVE_UNCONNECTED,
        NETWORK_ERROR,
        CONNECTION_LATENCY_UPDATED,
        BROADCAST,
        CONNECTION_REQUEST,
        MESSAGE_DELIVERED,
        PEER_ADDRESS_CHANGED
    };

    enum class DISCONNECT_REASON {
        CONNECTION_FAILED,
        TIMEOUT,
        HOST_UNREACHABLE,
        NETWORK_UNREACHABLE,
        REMOTE_CONNECTION_CLOSE,
        DISCONNECT_PEER_CALLED,
        CONNECTION_REJECTED,
        INVALID_PROTOCOL,
        UNKNOWN_HOST,
        RECONNECT,
        PEER_TO_PEER_CONNECTION,
        PEER_NOT_FOUND
    };

    enum class DELIVERY_METHOD : uint8_t {
        UNRELIABLE = 4,
        RELIABLE_UNORDERED = 0,
        SEQUENCED = 1,
        RELIABLE_ORDERED = 2,
        RELIABLE_SEQUENCED = 3
    };

    enum class UNCONNECTED_MESSAGE_TYPE {
        BASIC,
        BROADCAST
    };

    enum class PACKET_PROPERTY : uint8_t {
        UNRELIABLE,
        CHANNELED,
        ACK,
        PING,
        PONG,
        CONNECT_REQUEST,
        CONNECT_ACCEPT,
        DISCONNECT,
        UNCONNECTED_MESSAGE,
        MTU_CHECK,
        MTU_OK,
        BROADCAST,
        MERGED,
        SHUTDOWN_OK,
        PEER_NOT_FOUND,
        INVALID_PROTOCOL,
        NAT_MESSAGE,
        EMPTY,

        COUNT
    };

    enum class CONNECTION_REQUEST_RESULT {
        NONE,
        ACCEPT,
        REJECT,
        REJECT_FORCE
    };

    enum class SHUTDOWN_RESULT {
        NONE,
        SUCCESS,
        WAS_CONNECTED
    };

    enum class CONNECTION_STATE : uint8_t {
        OUTGOING = 1 << 1,
        CONNECTED = 1 << 2,
        SHUTDOWN_REQUESTED = 1 << 3,
        DISCONNECTED = 1 << 4,
        ENDPOINT_CHANGE = 1 << 5,
        ANY = OUTGOING | CONNECTED | SHUTDOWN_REQUESTED | ENDPOINT_CHANGE
    };

    enum class DISCONNECT_RESULT {
        NONE,
        REJECT,
        DISCONNECT
    };

    enum class CONNECT_REQUEST_RESULT {
        NONE,
        P2P_LOSE,
        RECONNECTION,
        NEW_CONNECTION
    };

    enum class NTP_MODE {
        CLIENT = 3,
        SERVER = 4
    };

    enum class NTP_LEAP_INDICATOR {
        NO_WARNING,
        LAST_MINUTE_HAS_61_SECONDS,
        LAST_MINUTE_HAS_59_SECONDS,
        ALARM_CONDITION
    };

    enum class DATE_TIME_KIND : uint32_t {
        UNSPECIFIED = 0,
        UTC = 1,
        LOCAL = 2
    };
}
