#include <lnl/packets/net_connect_request_packet.h>
#include <lnl/net_manager.h>

void lnl::net_connect_request_packet::recycle(struct net_manager* manager) {
    if (!packet) {
        return;
    }

    manager->pool_recycle(packet);
    packet = nullptr;
}
