#include <lnl/net_peer.h>
#include <lnl/net_manager.h>
#include <lnl/net_connection_request.h>
#include <lnl/net_utils.h>
#include <lnl/packets/net_connect_accept_packet.h>
#include <lnl/channels/net_reliable_channel.h>
#include <lnl/channels/net_sequenced_channel.h>

lnl::net_peer::net_peer(lnl::net_manager* netManager, lnl::net_connection_request* request, int32_t id)
        : net_peer(netManager, request->m_remote_endpoint, id) {
    m_connect_time = request->m_internal_packet->connection_time;
    m_connect_number = request->m_internal_packet->connection_number;
    m_remote_id = request->m_internal_packet->peer_id;

    auto connectAcceptPacket = net_connect_accept_packet::make(m_connect_time, m_connect_number, id);
    netManager->send_raw_and_recycle(connectAcceptPacket, m_endpoint);

    m_connection_state = CONNECTION_STATE::CONNECTED;
}

lnl::net_peer::~net_peer() {
    for (auto channel: m_channels) {
        if (!channel) {
            continue;
        }

        delete channel;
    }
}

lnl::SHUTDOWN_RESULT lnl::net_peer::shutdown(const std::optional<std::vector<uint8_t>>& rejectData,
                                             size_t offset, size_t size, bool force) {
    net_mutex_guard guard(m_shutdown_mutex);

    if (m_connection_state == CONNECTION_STATE::DISCONNECTED ||
        m_connection_state == CONNECTION_STATE::SHUTDOWN_REQUESTED) {
        return SHUTDOWN_RESULT::NONE;
    }

    auto result = m_connection_state == CONNECTION_STATE::CONNECTED
                  ? SHUTDOWN_RESULT::WAS_CONNECTED
                  : SHUTDOWN_RESULT::SUCCESS;

    if (force) {
        m_connection_state = CONNECTION_STATE::DISCONNECTED;
        return result;
    }

    m_time_since_last_packet = 0;

    auto shutdownPacket = m_net_manager->pool_get_with_property(PACKET_PROPERTY::DISCONNECT, size);
    shutdownPacket->set_connection_number(m_connect_number);
    if (shutdownPacket->size() >= net_constants::POSSIBLE_MTU[0]) {
        m_net_manager->m_logger.log("Disconnect additional data size more than MTU!");
    } else if (rejectData && size > 0) {
        memcpy(&shutdownPacket->data()[9], &rejectData->data()[offset], size);
    }

    m_connection_state = CONNECTION_STATE::SHUTDOWN_REQUESTED;
    m_net_manager->send_raw_and_recycle(shutdownPacket, m_endpoint);

    return result;
}

lnl::DISCONNECT_RESULT lnl::net_peer::process_disconnect(lnl::net_packet* packet) {
    auto isConnected = m_connection_state == CONNECTION_STATE::CONNECTED ||
                       m_connection_state == CONNECTION_STATE::OUTGOING;

    if (isConnected && packet->size() >= 9 && *(int64_t*) &packet->data()[1] == m_connect_time &&
        packet->connection_number() == m_connect_number) {
        return m_connection_state == CONNECTION_STATE::CONNECTED
               ? DISCONNECT_RESULT::DISCONNECT
               : DISCONNECT_RESULT::REJECT;
    }

    return DISCONNECT_RESULT::NONE;
}

bool lnl::net_peer::process_connect_accept(const std::unique_ptr<net_connect_accept_packet>& packet) {
    if (m_connection_state != CONNECTION_STATE::OUTGOING) {
        return false;
    }

    if (packet->connection_time() != m_connect_time) {
        return false;
    }

    m_connect_number = packet->connection_number();
    m_remote_id = packet->peer_id();

    m_time_since_last_packet = 0;

    m_connection_state = CONNECTION_STATE::CONNECTED;

    return true;
}

void lnl::net_peer::process_packet(lnl::net_packet* packet) {
    auto isConnected = m_connection_state == CONNECTION_STATE::CONNECTED ||
                       m_connection_state == CONNECTION_STATE::OUTGOING;

    if (!isConnected) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    if (packet->property() == PACKET_PROPERTY::SHUTDOWN_OK) {
        if (m_connection_state == CONNECTION_STATE::SHUTDOWN_REQUESTED) {
            m_connection_state = CONNECTION_STATE::DISCONNECTED;
        }

        m_net_manager->pool_recycle(packet);
        return;
    }

    if (packet->connection_number() != m_connect_number) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    m_time_since_last_packet = 0;

    switch (packet->property()) {
        case PACKET_PROPERTY::MERGED: {
            auto pos = net_constants::HEADER_SIZE;

            while (pos < packet->size()) {
                uint16_t size = *(uint16_t*) &packet->data()[pos];
                pos += 2;

                if (packet->buffer_size() - pos < size) {
                    break;
                }

                auto mergedPacket = m_net_manager->pool_get_packet(size);
                memcpy(mergedPacket->data(), &packet->data()[pos], size);

                if (!mergedPacket->verify()) {
                    break;
                }

                pos += size;
                process_packet(mergedPacket);
            }

            m_net_manager->pool_recycle(packet);
            break;
        }

        case PACKET_PROPERTY::PING: {
            if (relative_sequence_number(packet->sequence(), m_pong_packet.sequence()) > 0) {
                *(int64_t*) &m_pong_packet.data()[3] = get_current_time();
                m_pong_packet.set_sequence(packet->sequence());
                m_net_manager->send_raw(&m_pong_packet, m_endpoint);
            }

            m_net_manager->pool_recycle(packet);
            break;
        }

        case PACKET_PROPERTY::PONG: {
            if (packet->sequence() == m_ping_packet.sequence()) {
                auto current = std::chrono::high_resolution_clock::now();
                auto elapsedMs = (int32_t) std::chrono::duration_cast<std::chrono::milliseconds>(
                        current - m_ping_timer).count();
                m_remote_delta = *(int64_t*) &packet->data()[3] +
                                 (elapsedMs * TICKS_PER_MILLISECOND) / 2 -
                                 get_current_time();
                update_roundtrip_time(elapsedMs);
                m_net_manager->connection_latency_updated(m_endpoint, elapsedMs / 2);
            }

            m_net_manager->pool_recycle(packet);
            break;
        }

        case PACKET_PROPERTY::ACK:
        case PACKET_PROPERTY::CHANNELED: {
            if (packet->channel_id() > m_channels.size()) {
                m_net_manager->pool_recycle(packet);
                break;
            }

            net_base_channel* channel = m_channels[packet->channel_id()];

            if (!channel && packet->property() != PACKET_PROPERTY::ACK) {
                channel = create_channel(packet->channel_id());
            }

            if (channel) {
                if (!channel->process_packet(packet)) {
                    m_net_manager->pool_recycle(packet);
                }
            } else {
                m_net_manager->pool_recycle(packet);
            }

            break;
        }

        case PACKET_PROPERTY::UNRELIABLE: {
            m_net_manager->create_receive_event(packet, DELIVERY_METHOD::UNRELIABLE, 0, net_constants::HEADER_SIZE,
                                                m_endpoint);
            break;
        }

        case PACKET_PROPERTY::MTU_CHECK:
        case PACKET_PROPERTY::MTU_OK: {
            process_mtu_packet(packet);
            break;
        }
    }
}

void lnl::net_peer::update_roundtrip_time(int32_t roundTripTime) {
    m_rtt += roundTripTime;
    m_rtt_count++;
    m_avg_rtt = m_rtt / m_rtt_count;
    m_resend_delay = 25. + m_avg_rtt * 2.1;
}

lnl::net_peer::net_peer(lnl::net_manager* netManager, const lnl::net_address& endpoint, int32_t id) : m_pong_packet(
        PACKET_PROPERTY::PONG, 0) {
    m_id = id;
    m_endpoint = endpoint;
    m_net_manager = netManager;

    reset_mtu();

    m_channels.resize(netManager->channels_count * net_constants::CHANNEL_TYPE_COUNT);
}

lnl::net_base_channel* lnl::net_peer::create_channel(uint8_t idx) {
    auto newChannel = m_channels[idx];

    if (newChannel) {
        return newChannel;
    }
    //InterlockedCompareExchangePointer()

    switch ((DELIVERY_METHOD) (idx % net_constants::CHANNEL_TYPE_COUNT)) {
        case DELIVERY_METHOD::RELIABLE_UNORDERED: {
            newChannel = new net_reliable_channel(this, false, idx);
            break;
        }

        case DELIVERY_METHOD::SEQUENCED: {
            newChannel = new net_sequenced_channel(this, false, idx);
            break;
        }

        case DELIVERY_METHOD::RELIABLE_ORDERED: {
            newChannel = new net_reliable_channel(this, true, idx);
            break;
        }

        case DELIVERY_METHOD::RELIABLE_SEQUENCED: {
            newChannel = new net_sequenced_channel(this, true, idx);
            break;
        }
    }

    auto prevChannel = (net_base_channel*) InterlockedCompareExchangePointer((void**) &m_channels[idx], newChannel,
                                                                             nullptr);

    if (prevChannel && prevChannel != newChannel) {
        delete newChannel;
        return prevChannel;
    }

    m_channels[idx] = newChannel;

    return newChannel;
}

void lnl::net_peer::process_mtu_packet(lnl::net_packet* packet) {
    if (packet->size() < net_constants::POSSIBLE_MTU[0]) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    auto receivedMtu = packet->get_value_at<int32_t>(1);
    auto endMtuCheck = packet->get_value_at<int32_t>(packet->size() - 4);

    if (receivedMtu != packet->size() ||
        receivedMtu != endMtuCheck ||
        receivedMtu > net_constants::MAX_PACKET_SIZE) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    if (packet->property() == PACKET_PROPERTY::MTU_CHECK) {
        m_mtu_check_attempts = 0;
        packet->set_property(PACKET_PROPERTY::MTU_OK);
        m_net_manager->send_raw_and_recycle(packet, m_endpoint);
        return;
    }

    if (receivedMtu <= m_mtu || m_finish_mtu) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    if (receivedMtu != net_constants::POSSIBLE_MTU[m_mtu_idx + 1]) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    set_mtu(m_mtu_idx + 1);

    if (m_mtu_idx == net_constants::POSSIBLE_MTU.size() - 1) {
        m_finish_mtu = true;
    }

    m_net_manager->pool_recycle(packet);
}

void lnl::net_peer::add_reliable_packet(lnl::DELIVERY_METHOD method, lnl::net_packet* packet) {
    if (!packet->is_fragmented()) {
        m_net_manager->create_receive_event(packet,
                                            method,
                                            (uint8_t) (packet->channel_id() / net_constants::CHANNEL_TYPE_COUNT),
                                            net_constants::CHANNELED_HEADER_SIZE, m_endpoint);
        return;
    }

    uint16_t packetFragId = packet->fragment_id();

    auto it = m_holded_fragments.find(packetFragId);

    if (it == m_holded_fragments.end()) {
        incoming_fragments fragments;
        fragments.fragments.resize(packet->total_fragments(), nullptr);
        fragments.channel_id = packet->channel_id();
        it = m_holded_fragments.emplace(packetFragId, fragments).first;
    }

    auto& incomingFragments = it->second;
    auto& fragments = incomingFragments.fragments;

    if (packet->fragment_part() >= fragments.size() ||
        fragments[packet->fragment_part()] != nullptr ||
        packet->channel_id() != incomingFragments.channel_id) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    fragments[packet->fragment_part()] = packet;
    incomingFragments.received_count++;
    incomingFragments.total_size += packet->size() - net_constants::FRAGMENTED_HEADER_TOTAL_SIZE;

    if (incomingFragments.received_count != fragments.size()) {
        return;
    }

    auto resultingPacket = m_net_manager->pool_get_packet(incomingFragments.total_size);

    size_t pos = 0;

    for (int i = 0; i < incomingFragments.received_count; ++i) {
        auto fragment = fragments[i];
        size_t writtenSize = fragment->size() - net_constants::FRAGMENTED_HEADER_TOTAL_SIZE;

        if (pos + writtenSize > resultingPacket->buffer_size()) {
            clear_holded_fragments(packetFragId);
            m_holded_fragments.erase(packetFragId);
            return;
        }

        if (fragment->size() > fragment->buffer_size()) {
            clear_holded_fragments(packetFragId);
            m_holded_fragments.erase(packetFragId);
            return;
        }

        memcpy(&resultingPacket->data()[pos],
               &fragment->data()[net_constants::FRAGMENTED_HEADER_TOTAL_SIZE],
               writtenSize);

        pos += writtenSize;

        m_net_manager->pool_recycle(fragment);
        fragments[i] = nullptr;
    }

    m_holded_fragments.erase(packetFragId);

    m_net_manager->create_receive_event(resultingPacket,
                                        method,
                                        (uint8_t) (packet->channel_id() / net_constants::CHANNEL_TYPE_COUNT),
                                        0, m_endpoint);
}

void lnl::net_peer::clear_holded_fragments(uint16_t fragmentId) {
    auto it = m_holded_fragments.find(fragmentId);

    if (it == m_holded_fragments.end()) {
        return;
    }

    for (auto packet: it->second.fragments) {
        if (packet == nullptr) {
            continue;
        }

        m_net_manager->pool_recycle(packet);
    }
}

void lnl::net_peer::send_user_data(lnl::net_packet* packet) {
    static const size_t sizeTreshold = 20;
    packet->set_connection_number(m_connect_number);
    auto mergedPacketSize = net_constants::HEADER_SIZE + packet->size() + 2;

    if (mergedPacketSize + sizeTreshold >= m_mtu) {
        m_net_manager->send_raw(packet, m_endpoint);
        return;
    }

    if (m_merge_pos + mergedPacketSize > m_mtu) {
        send_merged();
    }


}
