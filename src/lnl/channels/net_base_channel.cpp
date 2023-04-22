#include <lnl/channels/net_base_channel.h>
#include <lnl/net_peer.h>

void lnl::net_base_channel::add_to_peer_channel_send_queue() {
    if (InterlockedCompareExchange(&m_is_added_to_peer_channel_send_queue, 1, 0) == 0) {
        m_peer->add_to_reliable_channel_send_queue(this);
    }
}
