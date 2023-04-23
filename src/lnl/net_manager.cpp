#include <lnl/net_manager.h>
#include <lnl/net_constants.h>
#include <lnl/packets/net_connect_request_packet.h>
#include <lnl/packets/net_connect_accept_packet.h>

#include <MSWSock.h> //for SIO_UDP_CONNRESET

lnl::net_manager::net_manager(net_event_listener* listener)
        : m_listener(listener) {
    m_peers_array.resize(32);
}

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
        m_logger.log("WSAStartup failed with error: %p", result);
        return false;
    }

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (m_socket == INVALID_SOCKET) {
        m_logger.log("Failed to create socket with error: %p", WSAGetLastError());
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
        m_logger.log("Cannot set SO_RCVTIMEO to %lu", timeout);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_SNDTIMEO, timeout)) {
        m_logger.log("Cannot set SO_SNDTIMEO to %lu", timeout);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_RCVBUF, net_constants::SOCKET_BUFFER_SIZE)) {
        m_logger.log("Cannot set SO_RCVBUF to %lu", net_constants::SOCKET_BUFFER_SIZE);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_SNDBUF, net_constants::SOCKET_BUFFER_SIZE)) {
        m_logger.log("Cannot set SO_SNDBUF to %lu", net_constants::SOCKET_BUFFER_SIZE);
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
        m_logger.log("Cannot set SIO_UDP_CONNRESET to false: %p", WSAGetLastError());
        return false;
    }
#endif

    if (!set_socket_option(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, !reuse_address)) {
        m_logger.log("Cannot set SO_EXCLUSIVEADDRUSE to %d", !reuse_address);
        return false;
    }

    if (!set_socket_option(SOL_SOCKET, SO_REUSEADDR, reuse_address)) {
        m_logger.log("Cannot set SO_REUSEADDR to %d", reuse_address);
        return false;
    }

    if (!set_socket_option(IPPROTO_IP, IP_TTL, net_constants::SOCKET_TTL)) {
        m_logger.log("Cannot set IP_TTL to %i", net_constants::SOCKET_TTL);
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
        m_logger.log("Bind failed: %p", WSAGetLastError());
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
            m_logger.log("recvfrom failed: %p", WSAGetLastError());
            continue;
        }

        packet->resize(size);

        on_message_received(packet, addr);
    }
}

void lnl::net_manager::update_logic() {
    std::vector<net_address> peersToRemove;
    net_stopwatch stopwatch;
    stopwatch.start();

    while (m_running) {
        auto elapsed = stopwatch.milliseconds();
        elapsed = elapsed <= 0 ? 1 : elapsed;
        stopwatch.restart();

        for (auto netPeer = m_head_peer; netPeer; netPeer = netPeer->m_next_peer) {
            if (netPeer->connection_state() == CONNECTION_STATE::DISCONNECTED &&
                netPeer->m_time_since_last_packet > disconnect_timeout) {
                peersToRemove.push_back(netPeer->m_endpoint);
            } else {
                netPeer->update(elapsed);
            }
        }

        if (!peersToRemove.empty()) {
            net_mutex_guard guard(m_peers_mutex);
            for (auto& addr: peersToRemove) {
                remove_peer_internal(addr);
            }
            peersToRemove.clear();
        }

        auto sleepTime = update_time - stopwatch.milliseconds();

        if (sleepTime <= 0) {
            continue;
        }

        //consider timeBeginPeriod(1) for win32
        Sleep(sleepTime);
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

    packet->clear();

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
            broadcastEvent.remoteEndpoint = addr;
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
            unconnectedMsgEvent.remoteEndpoint = addr;
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
                    disconnect_peer_force(netPeer->m_endpoint, DISCONNECT_REASON::PEER_NOT_FOUND, 0, nullptr);
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
                disconnect_peer_force(netPeer->m_endpoint, DISCONNECT_REASON::INVALID_PROTOCOL, 0, nullptr);
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

                disconnect_peer_force(netPeer->m_endpoint,
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
        auto errorCode = WSAGetLastError();
#ifndef NDEBUG
        m_logger.log("sendto failed: %p", errorCode);
#endif
        switch (errorCode) {
            case WSAEHOSTUNREACH:
            case WSAENETUNREACH: {
                if (disconnect_on_unreachable) {
                    auto peer = try_get_peer(endpoint);

                    if (peer) {
                        disconnect_peer_force(endpoint,
                                              errorCode == WSAEHOSTUNREACH
                                              ? DISCONNECT_REASON::HOST_UNREACHABLE
                                              : DISCONNECT_REASON::NETWORK_UNREACHABLE,
                                              errorCode, nullptr);
                    }
                }

                net_event_create_args args;
                args.type = NET_EVENT_TYPE::NETWORK_ERROR;
                args.socketErrorCode = errorCode;
                args.errorMessage = "Socket error";
                args.remoteEndpoint = endpoint;

                create_event(args);

                return -1;
            }
        }

        return 0;
    }

    return result;
}

void lnl::net_manager::create_event(net_event_create_args& args) {
#define ASSIGN_EVT_FIELD(fld) (evt.fld = args.fld)

    net_event evt(this, args.readerSource);
    ASSIGN_EVT_FIELD(type);
    ASSIGN_EVT_FIELD(peer);
    ASSIGN_EVT_FIELD(remoteEndpoint);
    ASSIGN_EVT_FIELD(socketErrorCode);
    ASSIGN_EVT_FIELD(latency);
    ASSIGN_EVT_FIELD(disconnectReason);
    ASSIGN_EVT_FIELD(connectionRequest);
    ASSIGN_EVT_FIELD(deliveryMethod);
    ASSIGN_EVT_FIELD(channelNumber);
    ASSIGN_EVT_FIELD(errorMessage);
    ASSIGN_EVT_FIELD(userData);
    ASSIGN_EVT_FIELD(reader);

#undef ASSIGN_EVT_FIELD

    {
        net_mutex_guard guard(m_events_queue_mutex);
        m_events_produce_queue.push_back(evt);
    }
}

void lnl::net_manager::process_connect_request(lnl::net_address& addr,
                                               std::shared_ptr<net_peer>& peer,
                                               std::unique_ptr<net_connect_request_packet>& request) {
    if (peer) {
        auto processResult = peer->process_connect_request(request);

        switch (processResult) {
            case CONNECT_REQUEST_RESULT::RECONNECTION: {
                disconnect_peer_force(addr, DISCONNECT_REASON::RECONNECT, 0, nullptr);
                remove_peer(addr);
                break;
            }

            case CONNECT_REQUEST_RESULT::NEW_CONNECTION: {
                remove_peer(addr);
                break;
            }

            case CONNECT_REQUEST_RESULT::P2P_LOSE: {
                disconnect_peer_force(addr, DISCONNECT_REASON::PEER_TO_PEER_CONNECTION, 0, nullptr);
                remove_peer(addr);
                break;
            }
            default:
                return;
        }

        if (processResult != CONNECT_REQUEST_RESULT::P2P_LOSE) {
            request->connection_number = (uint8_t) ((peer->connect_number() + 1) %
                                                    net_constants::MAX_CONNECTION_NUMBER);
        }
    } else {
        m_logger.log("ConnectRequest Id: %lld, EP: %s", request->connection_time, addr.to_string().c_str());
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

    m_logger.log("Creating request event: %lld", req->m_internal_packet->connection_time);

    net_event_create_args requestEvent{};
    requestEvent.type = NET_EVENT_TYPE::CONNECTION_REQUEST;
    requestEvent.remoteEndpoint = addr;
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

    if (m_head_peer) {
        peer->m_next_peer = m_head_peer;
        m_head_peer->m_prev_peer = peer;
    }

    m_head_peer = peer;
    m_peers.emplace(peer->endpoint(), peer);

    if (peer->m_id >= m_peers_array.size()) {
        auto newSize = m_peers_array.size() * 2;
        while (peer->m_id >= newSize) {
            newSize *= 2;
        }
        m_peers_array.resize(newSize);
    }

    m_peers_array[peer->m_id] = peer;

    m_logger.log("Added peer %s %i", peer->m_endpoint.to_string().c_str(), peer->m_id);
}

void lnl::net_manager::disconnect_peer(const net_address& address, lnl::DISCONNECT_REASON reason,
                                       uint32_t socketErrorCode, bool force,
                                       const std::optional<std::vector<uint8_t>>& rejectData, size_t offset,
                                       size_t size, lnl::net_packet* eventData) {
    auto peer = try_get_peer(address);

    if (!peer) {
        return;
    }

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
    std::shared_ptr<net_peer> peer = try_get_peer(address);

    if (!peer) {
        return;
    }

    net_event_create_args latencyEvent{};
    latencyEvent.type = NET_EVENT_TYPE::CONNECTION_LATENCY_UPDATED;
    latencyEvent.peer = peer;
    latencyEvent.latency = latency;

    create_event(latencyEvent);
}

void lnl::net_manager::create_receive_event(lnl::net_packet* packet,
                                            lnl::DELIVERY_METHOD method,
                                            uint8_t channelNumber,
                                            size_t headerSize,
                                            const lnl::net_address& endpoint) {
    net_event_create_args args;
    args.type = NET_EVENT_TYPE::RECEIVE;
    args.deliveryMethod = method;
    args.channelNumber = channelNumber;
    args.peer = try_get_peer(endpoint);
    args.readerSource = packet;
    args.reader = std::make_optional<net_data_reader>(packet->data(), packet->size(), headerSize);

    create_event(args);
}

void lnl::net_manager::message_delivered(const lnl::net_address& address, void* userData) {
    std::shared_ptr<net_peer> peer = try_get_peer(address);

    if (!peer) {
        return;
    }

    net_event_create_args messageDeliveredEvent{};
    messageDeliveredEvent.type = NET_EVENT_TYPE::MESSAGE_DELIVERED;
    messageDeliveredEvent.peer = peer;

    create_event(messageDeliveredEvent);
}

void lnl::net_manager::remove_peer(const lnl::net_address& address) {
    net_mutex_guard guard(m_peers_mutex);
    remove_peer_internal(address);
}

std::shared_ptr<lnl::net_peer> lnl::net_manager::try_get_peer(const lnl::net_address& endpoint) {
    static std::shared_ptr<net_peer> NULL_PEER(nullptr);

    net_mutex_guard guard(m_peers_mutex);

    auto it = m_peers.find(endpoint);

    if (it == m_peers.end()) {
        return NULL_PEER;
    }

    return it->second;
}

void lnl::net_manager::remove_peer_internal(const lnl::net_address& address) {
    std::shared_ptr<net_peer> peer;

    auto it = m_peers.find(address);

    if (it != m_peers.end()) {
        peer = it->second;
    }

    if (m_peers.erase(address) == 0) {
        return;
    }

    if (!peer) {
        return;
    }

    m_logger.log("Removing %s %i peer", peer->endpoint().to_string().c_str(), peer->m_id);

    if (m_head_peer == peer) {
        m_head_peer = peer->m_next_peer;
    }

    if (peer->m_prev_peer) {
        peer->m_prev_peer->m_next_peer = peer->m_next_peer;
    }

    if (peer->m_next_peer) {
        peer->m_next_peer->m_prev_peer = peer->m_prev_peer;
    }

    peer->m_prev_peer = nullptr;

    m_peers_array[peer->m_id] = nullptr;
    m_peer_ids.push(peer->m_id);
}

int32_t lnl::net_manager::get_next_peer_id() {
    auto result = m_peer_ids.dequeue();

    if (result) {
        return *result;
    }

    return m_peer_id_counter++;
}

void lnl::net_manager::poll_events() {
    {
        net_mutex_guard guard(m_events_queue_mutex);
        std::swap(m_events_produce_queue, m_events_consume_queue);
    }

    for (auto& evt: m_events_consume_queue) {
        process_event(evt);
    }

    m_events_consume_queue.clear();
}

void lnl::net_manager::process_event(lnl::net_event& event) {
    switch (event.type) {
        case NET_EVENT_TYPE::CONNECT: {
            m_listener->on_peer_connected(event.peer);
            break;
        }

        case NET_EVENT_TYPE::DISCONNECT: {
            disconnect_info info;
            info.reason = event.disconnectReason;
            info.additional_data = event.reader;
            info.socket_error_code = event.socketErrorCode;
            m_listener->on_peer_disconnected(event.peer, info);
            break;
        }

        case NET_EVENT_TYPE::RECEIVE: {
            m_listener->on_network_receive(event.peer, *event.reader, event.channelNumber, event.deliveryMethod);
            break;
        }

        case NET_EVENT_TYPE::RECEIVE_UNCONNECTED: {
            m_listener->on_network_receive_unconnected(event.remoteAddr, *event.reader,
                                                       UNCONNECTED_MESSAGE_TYPE::BASIC);
            break;
        }

        case NET_EVENT_TYPE::BROADCAST: {
            m_listener->on_network_receive_unconnected(event.remoteAddr, *event.reader,
                                                       UNCONNECTED_MESSAGE_TYPE::BASIC);
            break;
        }

        case NET_EVENT_TYPE::NETWORK_ERROR: {
            m_listener->on_network_error(event.remoteAddr, event.socketErrorCode, event.errorMessage.value());
            break;
        }

        case NET_EVENT_TYPE::CONNECTION_LATENCY_UPDATED: {
            m_listener->on_network_latency_update(event.peer, event.latency);
            break;
        }

        case NET_EVENT_TYPE::CONNECTION_REQUEST: {
            m_listener->on_connection_request(event.connectionRequest);
            break;
        }

        case NET_EVENT_TYPE::MESSAGE_DELIVERED: {
            m_listener->on_message_delivered(event.peer, event.userData);
            break;
        }
    }

    if (auto_recycle) {
        event.recycle();
    }

    assert(event.m_recycled);
}

void lnl::net_manager::create_error_event(uint32_t socketErrorCode, const std::string& errorMessage) {
    net_event_create_args args;
    args.type = NET_EVENT_TYPE::NETWORK_ERROR;
    args.socketErrorCode = socketErrorCode;
    args.errorMessage = errorMessage;

    create_event(args);
}
