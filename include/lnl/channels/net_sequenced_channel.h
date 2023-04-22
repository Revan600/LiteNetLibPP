#pragma once

#include <lnl/channels/net_base_channel.h>

namespace lnl {
    class net_sequenced_channel final : public net_base_channel {
    public:
        net_sequenced_channel(net_peer* mPeer, bool mOrdered, uint8_t mId) : net_base_channel(mPeer),
                                                                             m_ordered(mOrdered), m_id(mId) {}

        bool process_packet(net_packet* packet) override {
            return false;
        }

    private:
        bool m_ordered;
        uint8_t m_id;
    };
}