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
