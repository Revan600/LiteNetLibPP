#pragma once

#include <lnl/net_address.h>
#include <lnl/net_mutex.h>
#include <lnl/net_queue.h>
#include <lnl/net_constants.h>
#include <lnl/net_stopwatch.h>
#include <lnl/net_data_writer.h>
#include <lnl/channels/net_base_channel.h>
#include <lnl/packets/net_connect_request_packet.h>
#include <atomic>
#include <memory>
#include <unordered_map>

namespace lnl {
    class net_peer {
        static constexpr int32_t SHUTDOWN_DELAY = 300;
        static constexpr int32_t MTU_CHECK_DELAY = 1000;
        static constexpr int32_t MAX_MTU_CHECK_ATTEMPTS = 4;

        std::shared_ptr<net_peer> m_next_peer;
        std::shared_ptr<net_peer> m_prev_peer;

        int32_t m_id;

        int32_t m_remote_id = 0;
        std::atomic<int32_t> m_time_since_last_packet = 0;

        //mtu
        size_t m_mtu_idx = 0;
        int32_t m_mtu = 0;
        int32_t m_mtu_check_attempts = 0;
        int32_t m_mtu_check_timer = 0;
        bool m_finish_mtu = false;
        net_mutex m_mtu_mutex;

        //ping at rtt
        net_stopwatch m_ping_timer;
        int32_t m_ping_send_timer = 0;
        int64_t m_remote_delta = 0;
        int32_t m_rtt = 0;
        int32_t m_avg_rtt = 0;
        int32_t m_rtt_count = 0;
        int32_t m_rtt_reset_timer = 0;
        double_t m_resend_delay = 27.;

        CONNECTION_STATE m_connection_state = CONNECTION_STATE::DISCONNECTED;

        //connection
        net_address m_endpoint;
        net_mutex m_shutdown_mutex;
        net_packet m_shutdown_packet;
        int32_t m_shutdown_timer = 0;
        int64_t m_connect_time = 0;
        uint8_t m_connect_number = 0;
        int32_t m_connect_timer = 0;
        int32_t m_connect_attempts = 0;
        net_packet m_pong_packet;
        net_packet m_ping_packet;
        std::unique_ptr<net_packet> m_connect_request_packet;
        std::unique_ptr<net_packet> m_connect_accept_packet;

        //channels
        std::queue<net_packet*> m_unreliable_channel;
        net_mutex m_unreliable_channel_mutex;
        net_queue<net_base_channel*> m_channel_send_queue;
        std::vector<class net_base_channel*> m_channels;

        //fragment
        struct incoming_fragments {
            std::vector<net_packet*> fragments;
            int32_t received_count = 0;
            size_t total_size = 0;
            uint8_t channel_id = 0;
        };

        int32_t m_fragment_id = 0;
        std::unordered_map<uint16_t, incoming_fragments> m_holded_fragments;
        std::unordered_map<uint16_t, uint16_t> m_delivered_fragments;

        //merging
        net_packet m_merge_data;
        size_t m_merge_pos = 0;
        int32_t m_merge_count = 0;

    protected:
        class net_manager* m_net_manager;

    public:
        net_peer(net_manager* netManager, const net_address& endpoint, int32_t id);

        //accept incoming constructor
        net_peer(net_manager* netManager, class net_connection_request* request, int32_t id);

        //connect to constructor
        net_peer(net_manager* netManager, const net_address& endpoint, int32_t id, uint8_t connectNum,
                 net_data_writer& connectData);

        ~net_peer();

        const net_address& endpoint() const {
            return m_endpoint;
        }

        CONNECTION_STATE connection_state() const {
            return m_connection_state;
        }

        int64_t connect_time() const {
            return m_connect_time;
        }

        uint8_t connect_number() const {
            return m_connect_number;
        }

        int32_t remote_id() const {
            return m_remote_id;
        }

        inline void send(net_data_writer& writer, DELIVERY_METHOD deliveryMethod) {
            send(writer, 0, deliveryMethod);
        }

        inline void send(net_data_writer& writer, uint8_t channelNumber, DELIVERY_METHOD deliveryMethod) {
            send_internal(writer.data(), 0, writer.size(), channelNumber, deliveryMethod, nullptr);
        }

    private:
        DISCONNECT_RESULT process_disconnect(net_packet* packet);

        bool process_connect_accept(const std::unique_ptr<class net_connect_accept_packet>& packet);

        CONNECT_REQUEST_RESULT process_connect_request(std::unique_ptr<net_connect_request_packet>& request);

        void process_packet(net_packet* packet);

        void process_mtu_packet(net_packet* packet);

        void update_mtu_logic(int32_t deltaTime);

        void update_roundtrip_time(int32_t roundTripTime);

        void reject(std::unique_ptr<net_connect_request_packet>& requestData,
                    const std::optional<std::vector<uint8_t>>& rejectData,
                    size_t offset, size_t size) {
            m_connect_time = requestData->connection_time;
            m_connect_number = requestData->connection_number;

            shutdown(rejectData, offset, size, false);
        }

        SHUTDOWN_RESULT shutdown(const std::optional<std::vector<uint8_t>>& rejectData,
                                 size_t offset, size_t size, bool force);

        void reset_mtu() {
            set_mtu(0);
        }

        void set_mtu(size_t mtuIdx) {
            m_mtu_idx = mtuIdx;
            m_mtu = net_constants::POSSIBLE_MTU[mtuIdx];
        }

        net_base_channel* create_channel(uint8_t idx);

        void add_reliable_packet(DELIVERY_METHOD method, net_packet* packet);

        void clear_holded_fragments(uint16_t fragmentId);

        void send_user_data(net_packet* packet);

        void send_merged();

        void recycle_and_deliver(net_packet* packet);

        void add_to_reliable_channel_send_queue(net_base_channel* channel) {
            m_channel_send_queue.push(channel);
        }

        void update(int32_t deltaTime);

        void send_internal(uint8_t* data, size_t offset, size_t size, uint8_t channelNumber,
                           DELIVERY_METHOD deliveryMethod, void* userData);

        friend class net_manager;

        friend class net_base_channel;

        friend class net_reliable_channel;

        friend class net_sequenced_channel;
    };
}