#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <Winsock2.h>
#include <lnl/net_data_reader.h>
#include <lnl/net_data_writer.h>
#include <lnl/net_packet.h>
#include <lnl/net_address.h>

namespace lnl {
    class net_connect_request_packet final {
    public:
        int64_t connection_time;
        uint8_t connection_number;
        net_address target_address;
        net_data_reader data;
        int32_t peer_id;

        net_connect_request_packet(int64_t connectionTime, uint8_t connectionNumber, const net_address& targetAddress,
                                   const net_data_reader& data, int32_t peerId) : connection_time(connectionTime),
                                                                                  connection_number(connectionNumber),
                                                                                  target_address(targetAddress),
                                                                                  data(data), peer_id(peerId) {}

        static int32_t get_protocol_id(net_packet* packet) {
            return *(int32_t*) &packet->data()[1];
        }

        static std::unique_ptr<net_connect_request_packet> from_data(net_packet* packet) {
            if (packet->connection_number() >= net_constants::MAX_CONNECTION_NUMBER) {
                return nullptr;
            }

            auto connectionTime = *(int64_t*) &packet->data()[5];
            auto peerId = *(int32_t*) &packet->data()[13];
            auto addrSize = packet->data()[net_constants::CONNECT_REQUEST_HEADER_SIZE - 1];

            if (addrSize != 16) {
                return nullptr;
            }

            net_address targetAddress(*(sockaddr_in*) &packet->data()[net_constants::CONNECT_REQUEST_HEADER_SIZE]);
            net_data_reader reader(packet->data(), packet->size(),
                                   net_constants::CONNECT_REQUEST_HEADER_SIZE + addrSize);


            return std::make_unique<net_connect_request_packet>(connectionTime, packet->connection_number(),
                                                                targetAddress, reader, peerId);
        }

        static net_packet* make(const net_data_writer& connectData, const net_address& address, int64_t connectTime,
                                int32_t localId) {
            static_assert(sizeof(address.raw) == 16);

            auto result = new net_packet(PACKET_PROPERTY::CONNECT_REQUEST,
                                         connectData.size() + sizeof(address.raw));

            result->set_value_at(net_constants::PROTOCOL_ID, 1);
            result->set_value_at(connectTime, 5);
            result->set_value_at(localId, 13);
            result->set_value_at(address.raw, net_constants::HEADER_SIZE - 1);

            result->copy_from(connectData.data(), 0, net_constants::HEADER_SIZE + sizeof(address.raw),
                              connectData.size());

            return result;
        }
    };
}