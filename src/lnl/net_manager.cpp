#include <lnl/net_manager.h>
#include <lnl/net_constants.h>
#include <lnl/packets/net_connect_request_packet.h>
#include <lnl/packets/net_connect_accept_packet.h>

#include <MSWSock.h>
#include <ws2ipdef.h>

lnl::net_manager::net_manager() = default;

lnl::net_manager::~net_manager() {
    m_running = false;
    m_receive_thread.join();
    m_logic_thread.join();
}

bool lnl::net_manager::start(uint16_t port) {
    struct sockaddr_in addr{};
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    return start(addr);
}

bool lnl::net_manager::start(const sockaddr_in& addr) {
    WSADATA wsaData;

    auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0) {
        m_logger.log("WSAStartup failed with error: {0}", result);
        return false;
    }

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (m_socket == INVALID_SOCKET) {
        m_logger.log("Failed to create socket with error: {0}", WSAGetLastError());
        return false;
    }

    if (!bind_socket(addr)) {
        m_logger.log("Cannot bind socket");
        return false;
    }

    m_running = true;

    m_receive_thread = std::thread(&net_manager::receive_logic, this);
    m_logic_thread = std::thread(&net_manager::update_logic, this);

    return true;
}

bool lnl::net_manager::bind_socket(const sockaddr_in& addr) {
    DWORD timeout = 500;

    if (!set_socket_option(SOL_SOCKET, SO_RCVTIMEO, timeout)) {
        m_logger.log("Cannot set SO_RCVTIMEO to {0}", timeout);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_SNDTIMEO, timeout)) {
        m_logger.log("Cannot set SO_SNDTIMEO to {0}", timeout);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_RCVBUF, net_constants::SOCKET_BUFFER_SIZE)) {
        m_logger.log("Cannot set SO_RCVBUF to {0}", net_constants::SOCKET_BUFFER_SIZE);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_SNDBUF, net_constants::SOCKET_BUFFER_SIZE)) {
        m_logger.log("Cannot set SO_SNDBUF to {0}", net_constants::SOCKET_BUFFER_SIZE);
        return false;
    }

#ifdef _WIN32
    DWORD connresetValue = 0; //false
    DWORD bytesReturned = 0;
    if (WSAIoctl(m_socket,
                 SIO_UDP_CONNRESET,
                 &connresetValue,
                 sizeof(DWORD),
                 nullptr,
                 0,
                 &bytesReturned,
                 nullptr,
                 nullptr) != 0) {
        m_logger.log("Cannot set SIO_UDP_CONNRESET to false: {0}", WSAGetLastError());
        return false;
    }
#endif

    if (!set_socket_option(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, !reuse_address)) {
        m_logger.log("Cannot set SO_EXCLUSIVEADDRUSE to {0}", !reuse_address);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_REUSEADDR, reuse_address)) {
        m_logger.log("Cannot set SO_REUSEADDR to {0}", reuse_address);
        return false;
    }

    if (!set_socket_option(IPPROTO_IP, IP_TTL, net_constants::SOCKET_TTL)) {
        m_logger.log("Cannot set IP_TTL to {0}", net_constants::SOCKET_TTL);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_BROADCAST, true)) {
        m_logger.log("Cannot set IP_TTL");
        return false;
    }

#ifdef __APPLE__
    if (!set_socket_option(IPPROTO_IP, IP_DONTFRAGMENT, true)) {
        m_logger.log("Cannot set IP_DONTFRAGMENT");
        return false;
    }
#endif

    if (bind(m_socket, (sockaddr*) &addr, sizeof addr) == SOCKET_ERROR) {
        m_logger.log("Bind failed: {0}", WSAGetLastError());
        return false;
    }

    return true;
}

void lnl::net_manager::receive_logic() {
    net_address addr;

    while (m_running) {
        if (get_socket_available_data() == 0 && !socket_poll()) {
            continue;
        }

        auto addrLen = (int) sizeof(addr.raw);
        auto packet = pool_get_packet(net_constants::MAX_PACKET_SIZE);
        auto size = recvfrom(m_socket,
                             (char*) packet->data(), net_constants::MAX_PACKET_SIZE,
                             0, (sockaddr*) &addr.raw, &addrLen);

        if (size == SOCKET_ERROR) {
            m_logger.log("recvfrom failed: {0}", WSAGetLastError());
            continue;
        }

        packet->resize(size);

        on_message_received(packet, addr);
    }
}

void lnl::net_manager::update_logic() {
    while (m_running) {

    }
}

lnl::net_packet* lnl::net_manager::pool_get_packet(size_t size) {
    net_packet* result;

    {
        net_mutex_guard guard(m_packet_pool_mutex);

        result = m_packet_pool_head;

        if (result == nullptr) {
            result = new net_packet();
        } else {
            m_packet_pool_head = m_packet_pool_head->m_next;
            --m_packet_pool_size;
        }
    }

    result->resize(size);

    return result;
}

void lnl::net_manager::pool_recycle(lnl::net_packet* packet) {
    if (!packet) {
        return;
    }

    if (packet->buffer_size() > net_constants::MAX_PACKET_SIZE || m_packet_pool_size >= packet_pool_size) {
        delete packet;
        return;
    }

    packet->data()[0] = 0;

    {
        net_mutex_guard guard(m_packet_pool_mutex);
        packet->m_next = m_packet_pool_head;
        m_packet_pool_head = packet;
        ++m_packet_pool_size;
    }
}

size_t lnl::net_manager::get_socket_available_data() const {
    u_long availableData;

    if (ioctlsocket(m_socket, FIONREAD, &availableData) == SOCKET_ERROR) {
        return 0;
    }

    return availableData;
}

bool lnl::net_manager::socket_poll() const {
#ifdef _WIN32
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_socket, &rfds);

    struct timeval timeout{};
    timeout.tv_sec = (int) (RECEIVE_POLLING_TIME / 1000000);
    timeout.tv_usec = (int) (RECEIVE_POLLING_TIME % 1000000);

    auto result = select((int32_t) (m_socket + 1), &rfds, nullptr, nullptr,
                         &timeout);

    return result > 0;
#else
    m_logger.log("{0} is not implemented for current platform!", __PRETTY_FUNCTION__);
    return false;
#endif
}

void lnl::net_manager::on_message_received(lnl::net_packet* packet, net_address& addr) {
    if (!packet->verify()) {
        pool_recycle(packet);
#ifndef NDEBUG
        m_logger.log("Received malformed packet!");
#endif
        return;
    }

    auto property = packet->property();

    m_logger.log("Received packet {0}", (int) property);

    switch (property) {
        case PACKET_PROPERTY::CONNECT_REQUEST: {
            if (net_connect_request_packet::get_protocol_id(packet) != net_constants::PROTOCOL_ID) {
                send_raw_and_recycle(pool_get_with_property(PACKET_PROPERTY::INVALID_PROTOCOL), addr);
                pool_recycle(packet);
                return;
            }
            break;
        }
        case PACKET_PROPERTY::BROADCAST: {
            if (!broadcast_receive_enabled) {
                pool_recycle(packet);
                return;
            }

            net_event_create_args broadcastEvent{};
            broadcastEvent.type = NET_EVENT_TYPE::BROADCAST;
            broadcastEvent.remoteAddr = addr;
            broadcastEvent.readerSource = packet;
            create_event(broadcastEvent);

            return;
        }
        case PACKET_PROPERTY::UNCONNECTED_MESSAGE: {
            if (!unconnected_messages_enabled) {
                pool_recycle(packet);
                return;
            }

            net_event_create_args unconnectedMsgEvent{};
            unconnectedMsgEvent.type = NET_EVENT_TYPE::RECEIVE_UNCONNECTED;
            unconnectedMsgEvent.remoteAddr = addr;
            unconnectedMsgEvent.readerSource = packet;
            create_event(unconnectedMsgEvent);

            return;
        }
    }

    std::shared_ptr<net_peer> netPeer;

    {
        net_mutex_guard lock(m_peers_mutex);
        auto it = m_peers.find(addr);

        if (it != m_peers.end()) {
            netPeer = it->second;
        }
    }

    switch (property) {
        case PACKET_PROPERTY::CONNECT_REQUEST: {
            auto connRequest = net_connect_request_packet::from_data(packet);

            if (connRequest) {
                process_connect_request(addr, netPeer, connRequest);
            }

            break;
        }

        case PACKET_PROPERTY::PEER_NOT_FOUND: {
            if (netPeer) { //local
                if (netPeer->connection_state() != CONNECTION_STATE::CONNECTED) {
                    pool_recycle(packet);
                    return;
                }

                if (packet->size() == 1) {
                    //first reply
                    //send NetworkChanged packet
                    netPeer->reset_mtu();
                    send_raw_and_recycle(net_connect_accept_packet::make_network_changed(netPeer.get()), addr);
                } else if (packet->size() == 2 && packet->data()[1] == 1) {
                    disconnect_peer_force(netPeer, DISCONNECT_REASON::PEER_NOT_FOUND, 0, nullptr);
                } else if (packet->size() > 1) {//remote
                    //todo: implement peer address change
                    pool_recycle(packet);

                    auto secondResponse = pool_get_with_property(PACKET_PROPERTY::PEER_NOT_FOUND, 1);
                    secondResponse->data()[1] = 1;
                    send_raw_and_recycle(secondResponse, addr);
                }
            }
            break;
        }

        case PACKET_PROPERTY::INVALID_PROTOCOL: {
            if (netPeer && netPeer->connection_state() == CONNECTION_STATE::OUTGOING) {
                disconnect_peer_force(netPeer, DISCONNECT_REASON::INVALID_PROTOCOL, 0, nullptr);
            }
            break;
        }

        case PACKET_PROPERTY::DISCONNECT: {
            if (netPeer) {
                DISCONNECT_RESULT disconnectResult = netPeer->process_disconnect(packet);

                if (disconnectResult == DISCONNECT_RESULT::NONE) {
                    pool_recycle(packet);
                    return;
                }

                disconnect_peer_force(netPeer,
                                      disconnectResult == DISCONNECT_RESULT::DISCONNECT
                                      ? DISCONNECT_REASON::REMOTE_CONNECTION_CLOSE
                                      : DISCONNECT_REASON::CONNECTION_REJECTED, 0, packet);
            } else {
                pool_recycle(packet);
            }

            send_raw_and_recycle(pool_get_with_property(PACKET_PROPERTY::SHUTDOWN_OK), addr);
            break;
        }

        case PACKET_PROPERTY::CONNECT_ACCEPT: {
            if (!netPeer) {
                pool_recycle(packet);
                return;
            }

            auto connAccept = net_connect_accept_packet::from_data(packet);

            if (connAccept && netPeer->process_connect_accept(connAccept)) {

                net_event_create_args connectAcceptEvt{};
                connectAcceptEvt.type = NET_EVENT_TYPE::RECEIVE_UNCONNECTED;
                connectAcceptEvt.peer = netPeer;

                create_event(connectAcceptEvt);
            }

            pool_recycle(packet);

            break;
        }

        default: {
            if (netPeer) {
                netPeer->process_packet(packet);
            } else {
                pool_recycle(packet);
                send_raw_and_recycle(pool_get_with_property(PACKET_PROPERTY::PEER_NOT_FOUND), addr);
            }
        }
    }
}

lnl::net_packet* lnl::net_manager::pool_get_with_property(lnl::PACKET_PROPERTY property, size_t size) {
    auto packet = pool_get_packet(net_packet::get_header_size(property) + size);
    packet->set_property(property);
    return packet;
}

int32_t lnl::net_manager::send_raw_and_recycle(lnl::net_packet* packet, net_address& endpoint) {
    auto result = send_raw(packet->data(), 0, packet->size(), endpoint);
    pool_recycle(packet);
    return result;
}

int32_t lnl::net_manager::send_raw(const uint8_t* data, size_t offset, size_t length, net_address& endpoint) {
    if (!m_running) {
        return 0;
    }

    auto result = sendto(m_socket,
                         (const char*) &data[offset], (int) length,
                         0,
                         (sockaddr*) &endpoint.raw, sizeof(sockaddr_in));

    if (result == SOCKET_ERROR) {
#ifndef NDEBUG
        m_logger.log("sendto failed: {0}", WSAGetLastError());
#endif
        //todo: implement disconnect and other shit
        return 0;
    }

    return result;
}

void lnl::net_manager::create_event(lnl::net_event_create_args& args) {
    m_logger.log("Received event {0}", (int) args.type);

    if (args.type == NET_EVENT_TYPE::CONNECTION_REQUEST && args.connectionRequest) {
        args.connectionRequest->accept();
    }
}

void lnl::net_manager::process_connect_request(lnl::net_address& addr,
                                               std::shared_ptr<net_peer>& peer,
                                               std::unique_ptr<net_connect_request_packet>& request) {
    if (peer) {

    } else {
        m_logger.log("ConnectRequest Id: {0}, EP: {1}", request->connection_time, addr.to_string());
    }

    std::shared_ptr<net_connection_request> req;

    {
        net_mutex_guard guard(m_connection_requests_mutex);

        auto it = m_connection_requests.find(addr);

        if (it != m_connection_requests.end()) {
            it->second->update_request(request);
            return;
        }

        auto pair = m_connection_requests.emplace(addr, std::make_shared<net_connection_request>(this, request, addr));
        req = pair.first->second;
    }

    m_logger.log("Creating request event: {0}", req->m_internal_packet->connection_time);

    net_event_create_args requestEvent{};
    requestEvent.type = NET_EVENT_TYPE::CONNECTION_REQUEST;
    requestEvent.remoteAddr = addr;
    requestEvent.connectionRequest = req;
    create_event(requestEvent);
}

std::shared_ptr<lnl::net_peer> lnl::net_manager::on_connection_solved(
        lnl::net_connection_request* request,
        const std::optional<std::vector<uint8_t>>& rejectData,
        size_t offset, size_t size) {
    std::shared_ptr<lnl::net_peer> result;

    if (request->m_result == CONNECTION_REQUEST_RESULT::REJECT_FORCE) {
        if (rejectData && size > 0) {
            auto shutdownPacket = pool_get_with_property(PACKET_PROPERTY::DISCONNECT, size);
            shutdownPacket->set_connection_number(request->m_internal_packet->connection_number);
            if (shutdownPacket->size() >= net_constants::POSSIBLE_MTU[0]) {
                m_logger.log("Disconnect additional data size more than MTU!");
            } else {
                memcpy(&shutdownPacket->data()[9], &rejectData->data()[offset], size);
            }
            send_raw_and_recycle(shutdownPacket, request->m_remote_endpoint);
        }
    } else {
        net_mutex_guard guard(m_peers_mutex);

        auto it = m_peers.find(request->m_remote_endpoint);

        //if we don't have peer
        if (it == m_peers.end()) {
            if (request->m_result == CONNECTION_REQUEST_RESULT::REJECT) {
                result = std::make_shared<net_peer>(this, request->m_remote_endpoint, get_next_peer_id());
                result->reject(request->m_internal_packet, rejectData, offset, size);
                add_peer(result);
            } else {
                result = std::make_shared<net_peer>(this, request, get_next_peer_id());
                add_peer(result);
                guard.release();
                net_event_create_args requestEvent{};
                requestEvent.type = NET_EVENT_TYPE::CONNECT;
                requestEvent.peer = result;
                create_event(requestEvent);
            }
        }
    }

    {
        net_mutex_guard guard(m_connection_requests_mutex);
        m_connection_requests.erase(request->m_remote_endpoint);
    }

    return result;
}

void lnl::net_manager::add_peer(std::shared_ptr<net_peer>& peer) {
    net_mutex_guard guard(m_peers_mutex);

    m_peers.emplace(peer->endpoint(), peer);
}

void lnl::net_manager::disconnect_peer(std::shared_ptr<net_peer>& peer, lnl::DISCONNECT_REASON reason,
                                       uint32_t socketErrorCode, bool force,
                                       const std::optional<std::vector<uint8_t>>& rejectData, size_t offset,
                                       size_t size, lnl::net_packet* eventData) {
    auto shutdownResult = peer->shutdown(rejectData, offset, size, force);

    if (shutdownResult == SHUTDOWN_RESULT::NONE) {
        pool_recycle(eventData);
        return;
    }

    net_event_create_args disconnectEvent{};
    disconnectEvent.type = NET_EVENT_TYPE::DISCONNECT;
    disconnectEvent.peer = peer;
    disconnectEvent.socketErrorCode = socketErrorCode;
    disconnectEvent.disconnectReason = reason;
    disconnectEvent.readerSource = eventData;

    create_event(disconnectEvent);
}

void lnl::net_manager::connection_latency_updated(const net_address& address, int32_t latency) {
    std::shared_ptr<net_peer> peer;

    {
        net_mutex_guard guard(m_peers_mutex);
        auto it = m_peers.find(address);

        if (it != m_peers.end()) {
            peer = it->second;
        }
    }

    if (!peer) {
        return;
    }

    net_event_create_args latencyEvent{};
    latencyEvent.type = NET_EVENT_TYPE::CONNECTION_LATENCY_UPDATED;
    latencyEvent.peer = peer;
    latencyEvent.latency = latency;

    create_event(latencyEvent);
}

void lnl::net_manager::create_receive_event(lnl::net_packet* packet, lnl::DELIVERY_METHOD method, uint8_t channelNumber,
                                            size_t headerSize, const lnl::net_address& endpoint) {
    pool_recycle(packet);
}
