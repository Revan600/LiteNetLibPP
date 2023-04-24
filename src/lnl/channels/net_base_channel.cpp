#include <lnl/channels/net_base_channel.h>
#include <lnl/net_peer.h>
#include <lnl/net_manager.h>

void lnl::net_base_channel::add_to_peer_channel_send_queue() {
#ifdef WIN32
    if (InterlockedCompareExchange(&m_is_added_to_peer_channel_send_queue, 1, 0) == 0) {
        m_peer->add_to_reliable_channel_send_queue(this);
    }
#elif __linux__
    if (__sync_val_compare_and_swap(&m_is_added_to_peer_channel_send_queue, 0, 1) == 0) {
        m_peer->add_to_reliable_channel_send_queue(this);
    }
#endif
}

lnl::net_base_channel::~net_base_channel() {
    m_can_enqueue = false;

    if (!m_peer || m_outgoing_queue.empty()) {
        return;
    }

    std::optional<net_packet*> packet;

    while ((packet = m_outgoing_queue.dequeue())) {
        m_peer->m_net_manager->pool_recycle(*packet);
    }
}

void lnl::net_base_channel::add_to_queue(lnl::net_packet* packet) {
    if (!m_can_enqueue) {
        m_peer->m_net_manager->pool_recycle(packet);
        return;
    }

    m_outgoing_queue.push(packet);
    add_to_peer_channel_send_queue();
}
