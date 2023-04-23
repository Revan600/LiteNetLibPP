#pragma once

#include <cstdint>
#include <optional>
#include <thread>
#include <unordered_map>

#ifdef WIN32
#include <Winsock2.h>
#elif __linux__

#include <sys/socket.h>

#define SOCKET int32_t
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

#endif

#include <lnl/net_logger.h>
#include <lnl/net_packet.h>
#include <lnl/net_mutex.h>
#include <lnl/net_peer.h>
#include <lnl/net_event_listener.h>
#include <lnl/net_connection_request.h>
#include <lnl/net_address.h>

namespace lnl {
    class net_manager final {
        //constants
        static constexpr uint32_t RECEIVE_POLLING_TIME = 500000; //0.5 second

        bool m_running = false;
        SOCKET m_socket = INVALID_SOCKET;

        net_logger m_logger;

        std::thread m_receive_thread;
        std::thread m_logic_thread;

        net_mutex m_packet_pool_mutex;
        net_packet* m_packet_pool_head = nullptr;
        size_t m_packet_pool_size = 0;

        net_mutex m_peers_mutex;
        std::shared_ptr<net_peer> m_head_peer;
        std::unordered_map<net_address, std::shared_ptr<net_peer>, net_address_hash> m_peers;
        std::vector<std::shared_ptr<net_peer>> m_peers_array;
        net_queue<int32_t> m_peer_ids;
        std::atomic<int32_t> m_peer_id_counter = 0;

        net_mutex m_connection_requests_mutex;
        std::unordered_map<net_address, std::shared_ptr<net_connection_request>, net_address_hash> m_connection_requests;

        net_mutex m_events_queue_mutex;
        std::vector<net_event> m_events_consume_queue;
        std::vector<net_event> m_events_produce_queue;

        net_event_listener* m_listener;
    public:
#ifdef WIN32
        bool reuse_address = false;
#elif __linux__
        int32_t reuse_address = false;
#endif
        size_t packet_pool_size = 1000;
        bool broadcast_receive_enabled = false;
        bool unconnected_messages_enabled = false;
        uint8_t channels_count = 1;
        int32_t disconnect_timeout = 5000;
        int32_t update_time = 15;
        int32_t reconnect_delay = 500;
        int32_t max_connect_attempts = 10;
        int32_t ping_interval = 1000;
        bool auto_recycle = true;
        bool disconnect_on_unreachable = false;

        explicit net_manager(net_event_listener* listener);

        ~net_manager();

        [[nodiscard]] bool is_running() const {
            return m_running;
        }

        bool start(uint16_t port = 0);

        bool start(const sockaddr_in& addr);

        std::shared_ptr<net_peer> connect(const net_address& address, const net_data_writer& data);

        void poll_events();

        net_packet* pool_get_packet(size_t size);

        net_packet* pool_get_with_property(PACKET_PROPERTY property, size_t size = 0);

        void pool_recycle(net_packet* packet);

    private:
        struct net_event_create_args final {
            NET_EVENT_TYPE type = NET_EVENT_TYPE::CONNECT;
            std::shared_ptr<net_peer> peer;
            net_address remoteEndpoint;
            uint32_t socketErrorCode = 0;
            int32_t latency = 0;
            DISCONNECT_REASON disconnectReason = DISCONNECT_REASON::CONNECTION_FAILED;
            std::shared_ptr<net_connection_request> connectionRequest = {};
            DELIVERY_METHOD deliveryMethod = DELIVERY_METHOD::UNRELIABLE;
            uint8_t channelNumber = 0;
            net_packet* readerSource = nullptr;
            std::optional<net_data_reader> reader;
            std::optional<std::string> errorMessage;
            void* userData = nullptr;
        };

        //methods
        bool bind_socket(const sockaddr_in& addr);

        template <typename T>
        inline bool set_socket_option(int level, int option, T& value) {
            return setsockopt(m_socket, level, option, (char*) &value, sizeof(T)) == 0;
        }

        template <typename T>
        inline bool set_socket_option(int level, int option, T&& value) {
            return set_socket_option(level, option, value);
        }

        size_t get_socket_available_data() const;

        bool socket_poll();

        void receive_logic();

        void update_logic();

        void on_message_received(net_packet* packet, net_address& addr);

        void create_event(net_event_create_args& args);

        void process_event(net_event& event);

        void process_connect_request(net_address& addr,
                                     std::shared_ptr<net_peer>& peer,
                                     std::unique_ptr<net_connect_request_packet>& request);

        std::shared_ptr<net_peer> on_connection_solved(net_connection_request* request,
                                                       const std::optional<std::vector<uint8_t>>& rejectData,
                                                       size_t offset, size_t size);

        int32_t get_next_peer_id();

        std::shared_ptr<net_peer> try_get_peer(const net_address& endpoint);

        void add_peer(std::shared_ptr<net_peer>& peer);

        void remove_peer(const net_address& address);

        void remove_peer_internal(const net_address& address);

        void disconnect_peer_force(const net_address& address,
                                   DISCONNECT_REASON reason,
                                   uint32_t socketErrorCode,
                                   net_packet* eventData) {
            disconnect_peer(address, reason, socketErrorCode, true, {}, 0, 0, eventData);
        }

        void disconnect_peer(const net_address& address,
                             DISCONNECT_REASON reason,
                             uint32_t socketErrorCode,
                             bool force,
                             const std::optional<std::vector<uint8_t>>& rejectData,
                             size_t offset, size_t size, net_packet* eventData);

        void connection_latency_updated(const net_address& address, int32_t latency);

        void create_receive_event(net_packet* packet, DELIVERY_METHOD method, uint8_t channelNumber, size_t headerSize,
                                  const net_address& endpoint);

        void create_error_event(uint32_t socketErrorCode, const std::string& errorMessage);

        void message_delivered(const net_address& address, void* userData);

        //send methods
        int32_t send_raw_and_recycle(net_packet* packet, net_address& endpoint);

        inline int32_t send_raw(const net_packet* packet, net_address& address) {
            return send_raw(packet->data(), 0, packet->size(), address);
        }

        int32_t send_raw(const uint8_t* data, size_t offset, size_t length, net_address& endpoint);

        friend class net_connection_request;

        friend class net_peer;

        friend class net_sequenced_channel;
    };
}