#include <lnl/channels/net_sequenced_channel.h>
#include <lnl/net_peer.h>
#include <lnl/net_manager.h>
#include <lnl/net_utils.h>

bool lnl::net_sequenced_channel::process_packet(net_packet* packet) {
    if (packet->is_fragmented()) {
        return false;
    }

    if (packet->property() == PACKET_PROPERTY::ACK) {
        if (m_reliable && m_last_packet && packet->sequence() == m_last_packet->sequence()) {
            m_peer->m_net_manager->pool_recycle(m_last_packet);
            m_last_packet = nullptr;
        }

        return false;
    }

    auto relative = relative_sequence_number(packet->sequence(), m_remote_sequence);
    bool packetProcessed = false;

    if (packet->sequence() < net_constants::MAX_SEQUENCE && relative > 0) {
        m_remote_sequence = packet->sequence();
        m_peer->m_net_manager->create_receive_event(packet,
                                                    m_reliable ? DELIVERY_METHOD::RELIABLE_SEQUENCED
                                                               : DELIVERY_METHOD::SEQUENCED,
                                                    (uint8_t) (packet->channel_id() /
                                                               net_constants::CHANNEL_TYPE_COUNT),
                                                    net_constants::CHANNELED_HEADER_SIZE,
                                                    m_peer->m_endpoint);
        packetProcessed = true;
    }

    if (m_reliable) {
        m_must_send_ack = true;
        add_to_peer_channel_send_queue();
    }

    return packetProcessed;
}

bool lnl::net_sequenced_channel::send_next_packets() {
    if (m_reliable && m_outgoing_queue.empty()) {
        auto currentTime = get_current_time();
        auto packetHoldTime = currentTime - m_last_packet_send_time;

        if ((double) packetHoldTime >= m_peer->m_resend_delay * TICKS_PER_MILLISECOND) {
            if (m_last_packet) {
                m_last_packet_send_time = currentTime;
                m_peer->send_user_data(m_last_packet);
            }
        }
    } else {
        std::optional<net_packet*> packet;
        while ((packet = m_outgoing_queue.dequeue())) {
            m_local_sequence = (m_local_sequence + 1) % net_constants::MAX_SEQUENCE;
            packet.value()->set_sequence((uint16_t) m_local_sequence);
            packet.value()->set_channel_id(m_id);
            m_peer->send_user_data(packet.value());

            if (m_reliable && m_outgoing_queue.empty()) {
                m_last_packet_send_time = get_current_time();
                m_last_packet = packet.value();
            } else {
                m_peer->m_net_manager->pool_recycle(packet.value());
            }
        }
    }

    if (m_reliable && m_must_send_ack) {
        m_must_send_ack = false;
        m_ack_packet->set_sequence(m_remote_sequence);
        m_peer->send_user_data(m_ack_packet.get());
    }

    return m_last_packet != nullptr;
}
