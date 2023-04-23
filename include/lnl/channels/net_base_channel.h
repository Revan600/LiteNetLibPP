#pragma once

#include <lnl/net_packet.h>
#include <lnl/net_queue.h>
#include <lnl/net_utils.h>

#ifdef __linux__

#endif

namespace lnl {
    class net_base_channel {
    public:
        explicit net_base_channel(class net_peer* mPeer) : m_peer(mPeer) {}

        virtual ~net_base_channel() = default;

        virtual bool process_packet(net_packet* packet) = 0;

        bool send_and_check_queue() {
            auto hasPacketsToSend = send_next_packets();

            if (!hasPacketsToSend) {
#ifdef WIN32
                InterlockedExchange(&m_is_added_to_peer_channel_send_queue, 0);
#elif __linux__
                __sync_lock_test_and_set(&m_is_added_to_peer_channel_send_queue, 0);
#endif
            }

            return hasPacketsToSend;
        }

        void add_to_queue(net_packet* packet) {
            m_outgoing_queue.push(packet);
            add_to_peer_channel_send_queue();
        }

    protected:
        virtual bool send_next_packets() = 0;

        void add_to_peer_channel_send_queue();

        net_peer* m_peer;
        net_queue<net_packet*> m_outgoing_queue;

    private:
        uint32_t m_is_added_to_peer_channel_send_queue = 0;
    };
}