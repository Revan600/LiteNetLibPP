#include <lnl/net_structs.h>
#include <lnl/net_manager.h>

void lnl::net_event::recycle() {
    m_recycled = true;

    if (!m_reader_source) {
        return;
    }

    m_manager->pool_recycle(m_reader_source);
}