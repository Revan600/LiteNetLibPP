#pragma once

#include <memory>
#include <atomic>

#include <lnl/net_enums.h>
#include <lnl/packets/net_connect_request_packet.h>
#include <lnl/net_packet_reader.h>
#include <lnl/net_peer.h>

namespace lnl {
    class net_connection_request final {
        class net_manager* m_listener = nullptr;

        std::atomic<int32_t> m_used = 0;

        std::unique_ptr<net_connect_request_packet> m_internal_packet;
        net_address m_remote_endpoint;

        CONNECTION_REQUEST_RESULT m_result = CONNECTION_REQUEST_RESULT::NONE;

    private:
    public:
        net_connection_request(net_manager* listener,
                               std::unique_ptr<net_connect_request_packet>& internalPacket,
                               net_address& remoteEndpoint) : m_listener(listener),
                                                              m_remote_endpoint(remoteEndpoint) {
            m_internal_packet = std::move(internalPacket);
        }

        std::shared_ptr<net_peer> accept_if_key(const std::string& expectedKey);

        std::shared_ptr<net_peer> accept();

        void reject(const std::optional<std::vector<uint8_t>>& rejectData,
                    size_t offset, size_t size, bool force);

    private:

        void update_request(std::unique_ptr<net_connect_request_packet>& connectRequest) {
            if (connectRequest->connection_time < m_internal_packet->connection_time) {
                return;
            }

            if (connectRequest->connection_time == m_internal_packet->connection_time &&
                connectRequest->connection_number == m_internal_packet->connection_number) {
                return;
            }

            m_internal_packet = std::move(connectRequest);
        }

        bool try_activate() {
            int32_t expected = 0;
            int32_t desired = 1;
            return m_used.compare_exchange_strong(expected, desired);
        }

        friend class net_manager;

        friend class net_peer;
    };
}