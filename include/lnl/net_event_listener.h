#pragma once

#include <lnl/net_peer.h>
#include <lnl/net_structs.h>
#include <lnl/net_data_reader.h>
#include <lnl/net_connection_request.h>
#include <memory>

namespace lnl {
    class net_event_listener {
    public:
        virtual void on_peer_connected(std::shared_ptr<net_peer>& peer) {};

        virtual void on_peer_disconnected(std::shared_ptr<net_peer>& peer, disconnect_info& disconnectInfo) {};

        virtual void on_network_error(const net_address& endpoint, uint32_t socketErrorCode,
                                      const std::string& message) {};

        virtual void on_network_receive(std::shared_ptr<net_peer>& peer, net_data_reader& reader,
                                        uint8_t channelNumber, DELIVERY_METHOD deliveryMethod) {};

        virtual void on_network_receive_unconnected(const net_address& endpoint, net_data_reader& reader,
                                                    UNCONNECTED_MESSAGE_TYPE messageType) {};

        virtual void on_network_latency_update(std::shared_ptr<net_peer>& peer, int latency) {};

        virtual void on_connection_request(std::shared_ptr<net_connection_request>& request) {};

        virtual void on_message_delivered(std::shared_ptr<net_peer>& peer, void* userData) {};
    };
}