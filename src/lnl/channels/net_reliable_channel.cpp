#include <lnl/channels/net_reliable_channel.h>
#include <lnl/net_peer.h>
#include <lnl/net_utils.h>

bool lnl::net_reliable_channel::process_packet(lnl::net_packet* packet) {
    if (packet->property() == PACKET_PROPERTY::ACK) {
        process_ack(packet);
        return false;
    }

    auto seq = packet->sequence();

    if (seq >= net_constants::MAX_SEQUENCE) {
        return false;
    }

    auto relate = relative_sequence_number(seq, m_remote_window_start);
    auto relateSeq = relative_sequence_number(seq, m_remote_sequence);

    if (relateSeq > m_window_size) {
        return false;
    }

    if (relate < 0) {
        return false;
    }

    if (relate >= m_window_size * 2) {
        return false;
    }

    int32_t ackIdx;
    int32_t ackByte;
    int32_t ackBit;

    {
        net_mutex_guard guard(m_outgoing_acks_mutex);
        if (relate >= m_window_size) {
            //New window position
            int32_t newWindowStart = (m_remote_window_start + relate - m_window_size + 1) % net_constants::MAX_SEQUENCE;
            m_outgoing_acks.set_sequence(newWindowStart);

            //Clean old data
            while (m_remote_window_start != newWindowStart) {
                ackIdx = m_remote_window_start % m_window_size;
                ackByte = net_constants::CHANNELED_HEADER_SIZE + ackIdx / BITS_IN_BYTE;
                ackBit = ackIdx % BITS_IN_BYTE;
                m_outgoing_acks.data()[ackByte] &= (uint8_t) ~(1 << ackBit);
                m_remote_window_start = (m_remote_window_start + 1) % net_constants::MAX_SEQUENCE;
            }
        }

        //Final stage - process valid packet
        //trigger acks send
        m_must_send_acks = true;

        ackIdx = seq % m_window_size;
        ackByte = net_constants::CHANNELED_HEADER_SIZE + ackIdx / BITS_IN_BYTE;
        ackBit = ackIdx % BITS_IN_BYTE;
        if ((m_outgoing_acks.data()[ackByte] & (1 << ackBit)) != 0) {
            add_to_peer_channel_send_queue();
            return false;
        }

        //save ack
        m_outgoing_acks.data()[ackByte] |= (uint8_t) (1 << ackBit);
    }

    add_to_peer_channel_send_queue();

    //detailed check
    if (seq == m_remote_sequence) {
        m_peer->add_reliable_packet(m_delivery_method, packet);
        m_remote_sequence = (m_remote_sequence + 1) % net_constants::MAX_SEQUENCE;

        if (m_ordered) {
            net_packet* p;
            while ((p = m_received_packets[m_remote_sequence % m_window_size]) != nullptr) {
                //process holden packet
                m_received_packets[m_remote_sequence % m_window_size] = nullptr;
                m_peer->add_reliable_packet(m_delivery_method, p);
                m_remote_sequence = (m_remote_sequence + 1) % net_constants::MAX_SEQUENCE;
            }
        } else {
            while (m_early_received[m_remote_sequence % m_window_size]) {
                //process early packet
                m_early_received[m_remote_sequence % m_window_size] = false;
                m_remote_sequence = (m_remote_sequence + 1) % net_constants::MAX_SEQUENCE;
            }
        }
        return true;
    }

    //holden packet
    if (m_ordered) {
        m_received_packets[ackIdx] = packet;
    } else {
        m_early_received[ackIdx] = true;
        m_peer->add_reliable_packet(m_delivery_method, packet);
    }

    return true;
}

void lnl::net_reliable_channel::process_ack(lnl::net_packet* packet) {
    if (packet->size() != m_outgoing_acks.size()) {
        return;
    }

    uint16_t ackWindowStart = packet->sequence();
    int32_t windowRel = relative_sequence_number(m_local_window_start, ackWindowStart);

    if (ackWindowStart >= net_constants::MAX_SEQUENCE || windowRel < 0) {
        return;
    }

    if (windowRel >= m_window_size) {
        return;
    }

    net_mutex_guard guard(m_pending_packets_mutex);
    for (auto pendingSeq = m_local_window_start;
         pendingSeq != m_local_sequence;
         pendingSeq = (pendingSeq + 1) % net_constants::MAX_SEQUENCE) {
        auto rel = relative_sequence_number(pendingSeq, ackWindowStart);

        if (rel >= m_window_size) {
            break;
        }

        auto pendingIdx = pendingSeq % m_window_size;
        auto currentByte = net_constants::CHANNELED_HEADER_SIZE + pendingIdx / BITS_IN_BYTE;
        auto currentBit = pendingIdx % BITS_IN_BYTE;

        if ((packet->data()[currentByte] & (1 << currentBit)) == 0) {
            continue;
        }

        if (pendingSeq == m_local_window_start) {
            m_local_window_start = (m_local_window_start + 1) % net_constants::MAX_SEQUENCE;
        }

        m_pending_packets[pendingIdx].clear(m_peer);
    }
}

bool lnl::net_reliable_channel::pending_packet::try_send(int64_t currentTime, net_peer* peer) {
    if (m_packet == nullptr) {
        return false;
    }

    if (m_is_sent) {
        auto resendDelay = peer->m_resend_delay * TICKS_PER_MILLISECOND;
        auto packetHoldTime = (double) (currentTime - m_timestamp);

        if (packetHoldTime < resendDelay) {
            return false;
        }
    }

    m_timestamp = currentTime;
    m_is_sent = true;

    peer->send_user_data(m_packet);

    return true;
}

bool lnl::net_reliable_channel::pending_packet::clear(lnl::net_peer* peer) {
    if (m_packet == nullptr) {
        return false;
    }

    peer->recycle_and_deliver(m_packet);
    m_packet = nullptr;

    return true;
}
