#pragma once

#include <lnl/channels/net_base_channel.h>
#include <memory>

namespace lnl {
    class net_sequenced_channel final : public net_base_channel {
    public:
        net_sequenced_channel(net_peer* peer, bool reliable, uint8_t id)
                : net_base_channel(peer),
                  m_reliable(reliable), m_id(id) {
            if (!reliable) {
                return;
            }

            m_ack_packet = std::make_unique<net_packet>(PACKET_PROPERTY::ACK, 0);
        }

        bool process_packet(net_packet* packet) override;

    private:
        bool send_next_packets() override;

        bool m_reliable;
        uint8_t m_id;

        int32_t m_local_sequence = 0;
        uint16_t m_remote_sequence = 0;
        std::unique_ptr<net_packet> m_ack_packet;
        net_packet* m_last_packet = nullptr;
        int64_t m_last_packet_send_time = 0;
        bool m_must_send_ack = false;
    };
}