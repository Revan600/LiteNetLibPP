#include <lnl/net_peer.h>
#include <lnl/net_manager.h>
#include <lnl/net_connection_request.h>
#include <lnl/net_utils.h>
#include <lnl/packets/net_connect_accept_packet.h>
#include <lnl/channels/net_reliable_channel.h>
#include <lnl/channels/net_sequenced_channel.h>

lnl::net_peer::net_peer(lnl::net_manager* netManager, const lnl::net_address& endpoint, int32_t id)
        : m_connection_state(CONNECTION_STATE::CONNECTED),
          m_pong_packet(PACKET_PROPERTY::PONG, 0),
          m_ping_packet(PACKET_PROPERTY::PING, 0),
          m_merge_data(PACKET_PROPERTY::MERGED, net_constants::MAX_PACKET_SIZE),
          m_shutdown_packet(PACKET_PROPERTY::DISCONNECT, 0) {
    m_id = id;
    m_endpoint = endpoint;
    m_net_manager = netManager;

    reset_mtu();

    m_channels.resize(netManager->channels_count * net_constants::CHANNEL_TYPE_COUNT);
}

lnl::net_peer::net_peer(lnl::net_manager* netManager, lnl::net_connection_request* request, int32_t id)
        : net_peer(netManager, request->m_remote_endpoint, id) {
    m_connect_time = request->m_internal_packet->connection_time;
    m_connect_number = request->m_internal_packet->connection_number;
    m_remote_id = request->m_internal_packet->peer_id;

    m_connect_accept_packet = std::unique_ptr<net_packet>(
            net_connect_accept_packet::make(m_connect_time,
                                            m_connect_number, id));
    netManager->send_raw(m_connect_accept_packet.get(), m_endpoint);

    m_connection_state = CONNECTION_STATE::CONNECTED;
}

lnl::net_peer::net_peer(lnl::net_manager* netManager, const lnl::net_address& endpoint, int32_t id, uint8_t connectNum,
                        const net_data_writer& connectData)
        : net_peer(netManager, endpoint, id) {
    m_connect_time = get_current_time();
    m_connection_state = CONNECTION_STATE::OUTGOING;
    m_connect_number = connectNum;

    m_connect_request_packet = std::unique_ptr<net_packet>(
            net_connect_request_packet::make(connectData,
                                             endpoint, m_connect_time,
                                             id));
    m_connect_request_packet->set_connection_number(m_connect_number);

    m_net_manager->send_raw(m_connect_request_packet.get(), m_endpoint);
}


lnl::net_peer::~net_peer() {
    for (auto channel: m_channels) {
        if (!channel) {
            continue;
        }

        delete channel;
    }

    while (!m_unreliable_channel.empty()) {
        delete m_unreliable_channel.front();
        m_unreliable_channel.pop();
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

    m_shutdown_packet.resize(PACKET_PROPERTY::DISCONNECT, size);
    m_shutdown_packet.set_connection_number(m_connect_number);
    m_shutdown_packet.set_value_at(m_connect_time, 1);
    if (m_shutdown_packet.size() >= net_constants::POSSIBLE_MTU[0]) {
        m_net_manager->m_logger.log("Disconnect additional data size more than MTU!");
    } else if (rejectData && size > 0) {
        m_shutdown_packet.copy_from(rejectData->data(), offset, 9, size);
    }

    m_connection_state = CONNECTION_STATE::SHUTDOWN_REQUESTED;
    m_net_manager->send_raw(&m_shutdown_packet, m_endpoint);

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
                auto size = packet->get_value_at<uint16_t>(pos);
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
                m_pong_packet.set_value_at(get_current_time(), 3);
                m_pong_packet.set_sequence(packet->sequence());
                m_net_manager->send_raw(&m_pong_packet, m_endpoint);
            }

            m_net_manager->pool_recycle(packet);
            break;
        }

        case PACKET_PROPERTY::PONG: {
            if (packet->sequence() == m_ping_packet.sequence()) {
                m_ping_timer.stop();
                auto elapsedMs = m_ping_timer.milliseconds();
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

#ifdef WIN32
    auto prevChannel = (net_base_channel*) InterlockedCompareExchangePointer((void**) &m_channels[idx], newChannel,
                                                                             nullptr);
#elif __linux__
    auto prevChannel = (net_base_channel*) __sync_val_compare_and_swap((void**) &m_channels[idx], nullptr,
                                                                       newChannel);
#endif
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

    {
        net_mutex_guard guard(m_mtu_mutex);
        set_mtu(m_mtu_idx + 1);
    }

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

    uint8_t packetChannel = packet->channel_id();
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
                                        (uint8_t) (packetChannel / net_constants::CHANNEL_TYPE_COUNT),
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

    m_merge_data.set_value_at((uint16_t) packet->size(), m_merge_pos + net_constants::HEADER_SIZE);
    m_merge_data.copy_from(packet->data(), 0, m_merge_pos + net_constants::HEADER_SIZE + 2, packet->size());
    m_merge_pos += packet->size() + 2;
    m_merge_count++;
}

void lnl::net_peer::send_merged() {
    if (m_merge_count == 0) {
        return;
    }

    size_t offset = 0;
    size_t size = 0;

    if (m_merge_count > 1) {
        offset = 0;
        size = net_constants::HEADER_SIZE + m_merge_pos;
    } else {
        offset = net_constants::HEADER_SIZE + 2;
        size = m_merge_pos - 2;
    }

    m_net_manager->send_raw(m_merge_data.data(), offset, size, m_endpoint);

    m_merge_pos = 0;
    m_merge_count = 0;
}

void lnl::net_peer::recycle_and_deliver(lnl::net_packet* packet) {
    if (packet->user_data == nullptr) {
        m_net_manager->pool_recycle(packet);
        return;
    }

    if (packet->is_fragmented()) {
        auto it = m_delivered_fragments.find(packet->fragment_id());

        if (it != m_delivered_fragments.end()) {
            auto& fragCount = it->second;
            fragCount++;

            if (fragCount == packet->total_fragments()) {
                m_net_manager->message_delivered(m_endpoint, packet->user_data);
                m_delivered_fragments.erase(packet->fragment_id());
            }
        }
    } else {
        m_net_manager->message_delivered(m_endpoint, packet->user_data);
    }

    packet->user_data = nullptr;
    m_net_manager->pool_recycle(packet);
}

void lnl::net_peer::update(int32_t deltaTime) {
    m_time_since_last_packet += deltaTime;

    switch (m_connection_state) {
        case CONNECTION_STATE::CONNECTED: {
            if (m_time_since_last_packet > m_net_manager->disconnect_timeout) {
                m_net_manager->disconnect_peer_force(m_endpoint, DISCONNECT_REASON::TIMEOUT, 0, nullptr);
                return;
            }
            break;
        }

        case CONNECTION_STATE::SHUTDOWN_REQUESTED: {
            if (m_time_since_last_packet > m_net_manager->disconnect_timeout) {
                m_connection_state = CONNECTION_STATE::DISCONNECTED;
            } else {
                m_shutdown_timer += deltaTime;

                if (m_shutdown_timer >= SHUTDOWN_DELAY) {
                    m_shutdown_timer = 0;
                    m_net_manager->send_raw(&m_shutdown_packet, m_endpoint);
                }
            }

            return;
        }

        case CONNECTION_STATE::OUTGOING: {
            m_connect_timer += deltaTime;

            if (m_connect_timer > m_net_manager->reconnect_delay) {
                m_connect_timer = 0;
                m_connect_attempts++;

                if (m_connect_attempts > m_net_manager->max_connect_attempts) {
                    m_net_manager->disconnect_peer_force(m_endpoint, DISCONNECT_REASON::CONNECTION_FAILED, 0, nullptr);
                    return;
                }

                m_net_manager->send_raw(m_connect_request_packet.get(), m_endpoint);
            }

            return;
        }

        case CONNECTION_STATE::DISCONNECTED: {
            return;
        }
    }

    //send ping
    m_ping_send_timer += deltaTime;

    if (m_ping_send_timer >= m_net_manager->ping_interval) {
        m_ping_send_timer = 0;
        m_ping_packet.set_sequence(m_ping_packet.sequence() + 1);

        if (m_ping_timer.running()) {
            update_roundtrip_time((int32_t) m_ping_timer.milliseconds());
        }
        m_ping_timer.restart();
        m_net_manager->send_raw(&m_ping_packet, m_endpoint);
    }

    //RTT
    m_rtt_reset_timer += deltaTime;

    if (m_rtt_reset_timer > m_net_manager->ping_interval * 3) {
        m_rtt_reset_timer = 0;
        m_rtt = m_avg_rtt;
        m_rtt_count = 1;
    }

    update_mtu_logic(deltaTime);

    auto count = m_channel_send_queue.size();

    while (count-- > 0) {
        auto channel = m_channel_send_queue.dequeue();

        if (!channel) {
            break;
        }

        if (channel.value()->send_and_check_queue()) {
            m_channel_send_queue.push(*channel);
        }
    }

    {
        net_mutex_guard guard(m_unreliable_channel_mutex);

        while (!m_unreliable_channel.empty()) {
            auto packet = m_unreliable_channel.front();
            m_unreliable_channel.pop();
            send_user_data(packet);
            m_net_manager->pool_recycle(packet);
        }
    }

    send_merged();
}

lnl::CONNECT_REQUEST_RESULT lnl::net_peer::process_connect_request(
        std::unique_ptr<net_connect_request_packet>& request) {
    switch (m_connection_state) {
        case CONNECTION_STATE::OUTGOING: {
            if (request->connection_time < m_connect_time) {
                return CONNECT_REQUEST_RESULT::P2P_LOSE;
            }

            if (request->connection_time == m_connect_time) {
                auto remoteBytes = (uint8_t*) &m_endpoint.raw;
                auto localBytes = (uint8_t*) &request->target_address.raw;

                for (int i = sizeof(m_endpoint.raw) - 1; i >= 0; --i) {
                    auto rb = remoteBytes[i];

                    if (rb == localBytes[i])
                        continue;

                    if (rb < localBytes[i])
                        return CONNECT_REQUEST_RESULT::P2P_LOSE;
                }
            }
            break;
        }

        case CONNECTION_STATE::CONNECTED: {
            if (request->connection_time == m_connect_time) {
                m_net_manager->send_raw(m_connect_accept_packet.get(), m_endpoint);
            } else if (request->connection_time > m_connect_time) {
                return CONNECT_REQUEST_RESULT::RECONNECTION;
            }

            break;
        }

        case CONNECTION_STATE::DISCONNECTED:
        case CONNECTION_STATE::SHUTDOWN_REQUESTED: {
            if (request->connection_time >= m_connect_time) {
                return CONNECT_REQUEST_RESULT::NEW_CONNECTION;
            }
            break;
        }
    }

    return CONNECT_REQUEST_RESULT::NONE;
}

void lnl::net_peer::update_mtu_logic(int32_t deltaTime) {
    if (m_finish_mtu) {
        return;
    }

    m_mtu_check_timer += deltaTime;

    if (m_mtu_check_timer < MTU_CHECK_DELAY) {
        return;
    }

    m_mtu_check_timer = 0;
    m_mtu_check_attempts++;

    if (m_mtu_check_attempts >= MAX_MTU_CHECK_ATTEMPTS) {
        m_finish_mtu = true;
        return;
    }

    net_mutex_guard guard(m_mtu_mutex);

    if (m_mtu_idx >= net_constants::POSSIBLE_MTU.size() - 1) {
        return;
    }

    auto newMtu = net_constants::POSSIBLE_MTU[m_mtu_idx + 1];
    auto packet = m_net_manager->pool_get_packet(newMtu);
    packet->set_property(PACKET_PROPERTY::MTU_CHECK);
    packet->set_value_at(newMtu, 1);
    packet->set_value_at(newMtu, packet->size() - 4);

    if (m_net_manager->send_raw_and_recycle(packet, m_endpoint) <= 0) {
        m_finish_mtu = true;
    }
}

void lnl::net_peer::send_internal(const uint8_t* data, size_t offset, size_t size, uint8_t channelNumber,
                                  lnl::DELIVERY_METHOD deliveryMethod, void* userData) {
    if (m_connection_state != CONNECTION_STATE::CONNECTED || channelNumber >= m_channels.size()) {
        return;
    }

    PACKET_PROPERTY property;
    net_base_channel* channel = nullptr;

    if (deliveryMethod == DELIVERY_METHOD::UNRELIABLE) {
        property = PACKET_PROPERTY::UNRELIABLE;
    } else {
        property = PACKET_PROPERTY::CHANNELED;
        channel = create_channel(
                (uint8_t) (channelNumber * net_constants::CHANNEL_TYPE_COUNT + (uint8_t) deliveryMethod));
    }

    auto headerSize = net_packet::get_header_size(property);
    auto mtu = m_mtu;

    if (size + headerSize > mtu) {
        if (deliveryMethod != DELIVERY_METHOD::RELIABLE_ORDERED &&
            deliveryMethod != DELIVERY_METHOD::RELIABLE_UNORDERED) {
            m_net_manager->create_error_event(0,
                                              string_format(
                                                      "Unreliable or ReliableSequenced packet size exceeded maximum of %i bytes, Check allowed size by get_max_single_packet_size",
                                                      mtu - headerSize));
            return;
        }

        auto packetFullSize = mtu - headerSize;
        auto packetDataSize = packetFullSize - net_constants::FRAGMENT_HEADER_SIZE;
        auto totalPackets = size / packetDataSize + (size % packetDataSize == 0 ? 0 : 1);

        if (totalPackets >= UINT16_MAX) {
            m_net_manager->create_error_event(0,
                                              string_format("Data was split in %i fragments, which exceeds %i",
                                                            totalPackets, UINT16_MAX));
            return;
        }

#ifdef WIN32
        auto currentFragmentId = (uint16_t) InterlockedIncrement((uint32_t*) &m_fragment_id);
#elif __linux__
        auto currentFragmentId = (uint16_t) __sync_add_and_fetch((uint32_t*) &m_fragment_id, 1);
#endif

        for (uint16_t partIdx = 0; partIdx < totalPackets; partIdx++) {
            auto sendLength = size > packetDataSize ? packetDataSize : size;

            auto packet = m_net_manager->pool_get_packet(headerSize + sendLength + net_constants::FRAGMENT_HEADER_SIZE);
            packet->set_property(property);
            packet->user_data = userData;
            packet->set_fragment_id(currentFragmentId);
            packet->set_fragment_part(partIdx);
            packet->set_total_fragments((uint16_t) totalPackets);
            packet->mark_fragmented();

            packet->copy_from(data,
                              offset + partIdx * packetDataSize,
                              net_constants::FRAGMENTED_HEADER_TOTAL_SIZE,
                              sendLength);
            channel->add_to_queue(packet);

            size -= sendLength;
        }

        return;
    }

    auto packet = m_net_manager->pool_get_packet(headerSize + size);
    packet->set_property(property);
    packet->user_data = userData;
    packet->copy_from(data, offset, headerSize, size);

    if (channel == nullptr) {
        net_mutex_guard guard(m_unreliable_channel_mutex);
        m_unreliable_channel.push(packet);
    } else {
        channel->add_to_queue(packet);
    }
}
