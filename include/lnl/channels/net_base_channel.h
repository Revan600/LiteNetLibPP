#pragma once

#include <lnl/net_packet.h>
#include <lnl/net_queue.h>

namespace lnl {
    class net_base_channel {
    public:
        explicit net_base_channel(class net_peer* mPeer) : m_peer(mPeer) {}

        virtual ~net_base_channel() = default;

        virtual bool process_packet(net_packet* packet) = 0;

        //virtual bool send_next_packets() = 0;

    protected:
        void add_to_peer_channel_send_queue();

        net_peer* m_peer;
        net_queue<net_packet*> m_outgoing_queue;

    private:
        uint32_t m_is_added_to_peer_channel_send_queue = 0;
    };
}