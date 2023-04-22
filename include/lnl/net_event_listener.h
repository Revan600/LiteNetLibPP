#pragma once

#include <lnl/net_peer.h>
#include <lnl/net_structs.h>
#include <lnl/net_packet_reader.h>
#include <memory>
#include <winsock.h>

namespace lnl {
    template <typename TTag>
    class net_event_listener {
    public:
        virtual void on_peer_connected(std::unique_ptr<net_peer>& peer) = 0;

        virtual void on_peer_disconnected(std::unique_ptr<net_peer>& peer, disconnect_info& disconnectInfo) = 0;

        virtual void on_network_error(sockaddr* endpoint, uint32_t socketErrorCode) = 0;

        virtual void on_network_receive(std::unique_ptr<net_peer>& peer, net_packet_reader& reader,
                                        uint8_t channelNumber, DELIVERY_METHOD deliveryMethod) = 0;

        virtual void on_network_receive_unconnected(sockaddr* endpoint, net_packet_reader& reader,
                                                    UNCONNECTED_MESSAGE_TYPE messageType) = 0;
    };
}