#include <lnl/packets/net_connect_accept_packet.h>
#include <lnl/net_peer.h>

lnl::net_packet* lnl::net_connect_accept_packet::make(int64_t connectTime, uint8_t connectNum, int32_t localPeerId) {
    auto packet = new net_packet(PACKET_PROPERTY::CONNECT_ACCEPT, 0);
    *(int64_t*) &packet->data()[1] = connectTime;
    packet->data()[9] = connectNum;
    packet->data()[10] = 0;
    *(int32_t*) &packet->data()[11] = localPeerId;
    return packet;
}

lnl::net_packet* lnl::net_connect_accept_packet::make_network_changed(net_peer* peer) {
    auto packet = new net_packet(PACKET_PROPERTY::PEER_NOT_FOUND, net_constants::CONNECT_ACCEPT_HEADER_SIZE - 1);
    *(int64_t*) &packet->data()[1] = peer->connect_time();
    packet->data()[9] = peer->connect_number();
    packet->data()[10] = 1;
    *(int32_t*) &packet->data()[11] = peer->remote_id();
    return packet;
}

std::unique_ptr<lnl::net_connect_accept_packet> lnl::net_connect_accept_packet::from_data(lnl::net_packet* packet) {
    if (packet->size() != net_constants::CONNECT_ACCEPT_HEADER_SIZE) {
        return nullptr;
    }

    auto connectionTime = *(int64_t*) &packet->data()[1];
    auto connectionNumber = packet->data()[9];

    if (connectionNumber >= net_constants::MAX_CONNECTION_NUMBER) {
        return nullptr;
    }

    auto isReused = packet->data()[10];

    if (isReused > 1) {
        return nullptr;
    }

    auto peerId = *(int32_t*) &packet->data()[11];

    if (peerId < 0) {
        return nullptr;
    }

    return std::make_unique<net_connect_accept_packet>(connectionTime, connectionNumber, peerId, isReused == 1);
}
