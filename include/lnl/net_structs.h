#pragma once

#include <memory>
#include <optional>

#include <lnl/net_enums.h>
#include <lnl/net_address.h>
#include <lnl/net_data_reader.h>

namespace lnl {
    struct disconnect_info {
        DISCONNECT_REASON reason;
        std::optional<net_data_reader> additional_data;
        uint32_t socket_error_code;
    };

    struct net_event final {
        NET_EVENT_TYPE type = NET_EVENT_TYPE::CONNECT;
        std::shared_ptr<class net_peer> peer;
        net_address remoteAddr;
        uint32_t socketErrorCode = 0;
        int32_t latency = 0;
        DISCONNECT_REASON disconnectReason = DISCONNECT_REASON::CONNECTION_FAILED;
        std::shared_ptr<class net_connection_request> connectionRequest = {};
        DELIVERY_METHOD deliveryMethod = DELIVERY_METHOD::UNRELIABLE;
        uint8_t channelNumber = 0;
        std::optional<net_data_reader> reader;
        std::optional<std::string> errorMessage;
        void* userData = nullptr;

        void recycle();

    private:
        explicit net_event(class net_manager* manager, class net_packet* packet)
                : m_manager(manager), m_reader_source(packet) {}

        net_packet* m_reader_source = nullptr;

        net_manager* m_manager = nullptr;
        mutable bool m_recycled = false;

        friend class net_manager;
    };
}