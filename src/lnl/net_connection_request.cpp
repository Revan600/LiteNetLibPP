#include <lnl/net_connection_request.h>
#include <lnl/net_manager.h>

std::shared_ptr<lnl::net_peer> lnl::net_connection_request::accept_if_key(
        const std::string& expectedKey) {
    if (!try_activate()) {
        return {};
    }

    std::string key;

    if (m_internal_packet->data.try_read(key) && key == expectedKey) {
        m_result = CONNECTION_REQUEST_RESULT::ACCEPT;
    }

    if (m_result == CONNECTION_REQUEST_RESULT::ACCEPT) {
        return m_listener->on_connection_solved(this, {}, 0, 0);
    }

    m_result = CONNECTION_REQUEST_RESULT::REJECT;
    m_listener->on_connection_solved(this, {}, 0, 0);
    return {};
}

std::shared_ptr<lnl::net_peer> lnl::net_connection_request::accept() {
    if (!try_activate()) {
        return {};
    }

    m_result = CONNECTION_REQUEST_RESULT::ACCEPT;
    return m_listener->on_connection_solved(this, {}, 0, 0);
}

void lnl::net_connection_request::reject(const std::optional<std::vector<uint8_t>>& rejectData,
                                         size_t offset, size_t size,
                                         bool force) {
    if (!try_activate()) {
        return;
    }

    m_result = force ? CONNECTION_REQUEST_RESULT::REJECT_FORCE : CONNECTION_REQUEST_RESULT::REJECT;
    m_listener->on_connection_solved(this, rejectData, offset, size);
}
