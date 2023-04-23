#pragma once

#include <lnl/net_peer.h>
#include <lnl/net_structs.h>
#include <lnl/net_data_reader.h>
#include <lnl/net_connection_request.h>
#include <memory>
#include <winsock.h>

namespace lnl {
    class net_event_listener {
    public:
        virtual void on_peer_connected(std::shared_ptr<net_peer>& peer) = 0;

        virtual void on_peer_disconnected(std::shared_ptr<net_peer>& peer, disconnect_info& disconnectInfo) = 0;

        virtual void on_network_error(const net_address& endpoint, uint32_t socketErrorCode,
                                      const std::string& message) = 0;

        virtual void on_network_receive(std::shared_ptr<net_peer>& peer, net_data_reader& reader,
                                        uint8_t channelNumber, DELIVERY_METHOD deliveryMethod) = 0;

        virtual void on_network_receive_unconnected(const net_address& endpoint, net_data_reader& reader,
                                                    UNCONNECTED_MESSAGE_TYPE messageType) = 0;

        virtual void on_network_latency_update(std::shared_ptr<net_peer>& peer, int latency) = 0;

        virtual void on_connection_request(std::shared_ptr<net_connection_request>& request) = 0;

        virtual void on_message_delivered(std::shared_ptr<net_peer>& peer, void* userData) = 0;
    };
}