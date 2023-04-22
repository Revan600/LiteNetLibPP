#pragma once

#include <lnl/net_address.h>
#include <lnl/net_mutex.h>
#include <lnl/net_constants.h>
#include <lnl/channels/net_base_channel.h>
#include <lnl/packets/net_connect_request_packet.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_map>

namespace lnl {
    class net_peer {
        int32_t m_id;
        net_address m_endpoint;

        int64_t m_connect_time = 0;
        uint8_t m_connect_number = 0;
        int32_t m_remote_id = 0;
        std::atomic<int32_t> m_time_since_last_packet = 0;

        //mtu
        size_t m_mtu_idx = 0;
        int32_t m_mtu = 0;
        int32_t m_mtu_check_attempts = 0;
        bool m_finish_mtu = false;

        //ping at rtt
        std::chrono::time_point<std::chrono::steady_clock> m_ping_timer;
        int64_t m_remote_delta = 0;
        int32_t m_rtt = 0;
        int32_t m_avg_rtt = 0;
        int32_t m_rtt_count = 0;
        double_t m_resend_delay = 27.;

        CONNECTION_STATE m_connection_state = CONNECTION_STATE::DISCONNECTED;

        net_mutex m_shutdown_mutex;

        //system packets
        net_packet m_pong_packet;
        net_packet m_ping_packet;

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

    protected:
        class net_manager* m_net_manager;

    public:
        net_peer(net_manager* netManager, const net_address& endpoint, int32_t id);

        net_peer(net_manager* netManager, class net_connection_request* request, int32_t id);

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

    private:
        DISCONNECT_RESULT process_disconnect(net_packet* packet);

        bool process_connect_accept(const std::unique_ptr<class net_connect_accept_packet>& packet);

        void process_packet(net_packet* packet);

        void process_mtu_packet(net_packet* packet);

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

        friend class net_manager;

        friend class net_reliable_channel;
    };
}