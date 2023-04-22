#pragma once

#include <lnl/net_packet.h>
#include <memory>

namespace lnl {
    class net_connect_accept_packet final {
        int64_t m_connection_time;
        uint8_t m_connection_number;
        int32_t m_peer_id;
        bool m_peer_network_changed;

    public:
        net_connect_accept_packet(int64_t connectionTime, uint8_t connectionNumber, int32_t peerId,
                                  bool peerNetworkChanged) : m_connection_time(connectionTime),
                                                             m_connection_number(connectionNumber),
                                                             m_peer_id(peerId),
                                                             m_peer_network_changed(peerNetworkChanged) {}

        [[nodiscard]] int64_t connection_time() const {
            return m_connection_time;
        }

        [[nodiscard]] uint8_t connection_number() const {
            return m_connection_number;
        }

        [[nodiscard]] int32_t peer_id() const {
            return m_peer_id;
        }

        [[nodiscard]]  bool peer_network_changed() const {
            return m_peer_network_changed;
        }

        static net_packet* make(int64_t connectTime, uint8_t connectNum, int32_t localPeerId);

        static net_packet* make_network_changed(class net_peer* peer);

        static std::unique_ptr<net_connect_accept_packet> from_data(net_packet* packet);
    };
}