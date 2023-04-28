#pragma once

#include <lnl/net_event_listener.h>
#include <lnl/net_delegate.h>

namespace lnl {
    class net_event_based_listener : public net_event_listener {
#define DECLARE_EVENT(name, ...)             \
    private:                                 \
        net_delegate<__VA_ARGS__> m_##name;  \
    public:                                  \
        decltype(m_##name)& name() {         \
            return m_##name;                 \
        }

    DECLARE_EVENT(peer_connected, std::shared_ptr<net_peer> &);
    DECLARE_EVENT(peer_disconnected, std::shared_ptr<net_peer> &, disconnect_info &);
    DECLARE_EVENT(network_error, const net_address&, uint32_t,
                  const std::string&);
    DECLARE_EVENT(network_receive, std::shared_ptr<net_peer> &, net_data_reader &, uint8_t, DELIVERY_METHOD);
    DECLARE_EVENT(network_receive_unconnected, const net_address&, net_data_reader &, UNCONNECTED_MESSAGE_TYPE);
    DECLARE_EVENT(network_latency_update, std::shared_ptr<net_peer> &, int);
    DECLARE_EVENT(connection_request, std::shared_ptr<net_connection_request> &);
    DECLARE_EVENT(message_delivered, std::shared_ptr<net_peer> &, void*);

#undef DECLARE_EVENT
    protected:
        void on_peer_connected(std::shared_ptr<net_peer>& peer) override {
            m_peer_connected(peer);
        }

        void on_peer_disconnected(std::shared_ptr<net_peer>& peer, disconnect_info& disconnectInfo) override {
            m_peer_disconnected(peer, disconnectInfo);
        }

        void on_network_error(const net_address& endpoint, uint32_t socketErrorCode,
                              const std::string& message) override {
            m_network_error(endpoint, socketErrorCode, message);
        }

        void on_network_receive(std::shared_ptr<net_peer>& peer, net_data_reader& reader, uint8_t channelNumber,
                                DELIVERY_METHOD deliveryMethod) override {
            m_network_receive(peer, reader, channelNumber, deliveryMethod);
        }

        void on_network_receive_unconnected(const net_address& endpoint, net_data_reader& reader,
                                            UNCONNECTED_MESSAGE_TYPE messageType) override {
            m_network_receive_unconnected(endpoint, reader, messageType);
        }

        void on_network_latency_update(std::shared_ptr<net_peer>& peer, int latency) override {
            m_network_latency_update(peer, latency);
        }

        void on_connection_request(std::shared_ptr<net_connection_request>& request) override {
            m_connection_request(request);
        }

        void on_message_delivered(std::shared_ptr<net_peer>& peer, void* userData) override {
            m_message_delivered(peer, userData);
        }
    };
}