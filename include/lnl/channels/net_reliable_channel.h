#pragma once

#include <lnl/channels/net_base_channel.h>
#include <lnl/net_mutex.h>

namespace lnl {
    class net_reliable_channel final : public net_base_channel {
        static constexpr int32_t BITS_IN_BYTE = 8;

        class pending_packet {
            net_packet* m_packet = nullptr;
            int64_t m_timestamp = 0;
            bool m_is_sent = false;

        public:
            void init(net_packet* packet) {
                m_packet = packet;
                m_is_sent = false;
            }

            bool try_send(int64_t currentTime, net_peer* peer);

            bool clear(net_peer* peer);
        };

    public:
        net_reliable_channel(net_peer* peer, bool ordered, uint8_t id)
                : net_base_channel(peer),
                  m_ordered(ordered),
                  m_id(id),
                  m_window_size(net_constants::DEFAULT_WINDOW_SIZE),
                  m_outgoing_acks(PACKET_PROPERTY::ACK, (net_constants::DEFAULT_WINDOW_SIZE - 1) / BITS_IN_BYTE + 2) {
            m_outgoing_acks.set_channel_id(id);
            m_pending_packets.resize(m_window_size);

            if (ordered) {
                m_delivery_method = DELIVERY_METHOD::RELIABLE_ORDERED;
                m_received_packets.resize(m_window_size);
            } else {
                m_delivery_method = DELIVERY_METHOD::RELIABLE_UNORDERED;
                m_early_received.resize(m_window_size, false);
            }
        }

        bool process_packet(net_packet* packet) override;

    private:
        void process_ack(net_packet* packet);

        int32_t m_local_sequence = 0;
        int32_t m_remote_sequence = 0;
        int32_t m_local_window_start = 0;
        int32_t m_remote_window_start = 0;

        bool m_ordered;
        uint8_t m_id;
        int32_t m_window_size;

        bool m_must_send_acks = false;
        DELIVERY_METHOD m_delivery_method;

        net_mutex m_outgoing_acks_mutex;
        net_packet m_outgoing_acks;
        net_mutex m_pending_packets_mutex;
        std::vector<pending_packet> m_pending_packets;
        std::vector<net_packet*> m_received_packets;
        std::vector<bool> m_early_received;
    };
}