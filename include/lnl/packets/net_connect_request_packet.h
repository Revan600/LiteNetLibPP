#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <Winsock2.h>
#include <lnl/net_packet_reader.h>
#include <lnl/net_packet.h>
#include <lnl/net_address.h>

namespace lnl {
    class net_connect_request_packet final {
    public:
        int64_t connection_time;
        uint8_t connection_number;
        net_address target_address;
        net_packet_reader data;
        int32_t peer_id;

        net_connect_request_packet(int64_t connectionTime, uint8_t connectionNumber, const net_address& targetAddress,
                                   const net_packet_reader& data, int32_t peerId) : connection_time(connectionTime),
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
            net_packet_reader reader(packet->data(), packet->size(),
                                     net_constants::CONNECT_REQUEST_HEADER_SIZE + addrSize);


            return std::make_unique<net_connect_request_packet>(connectionTime, packet->connection_number(),
                                                                targetAddress, reader, peerId);
        }
    };
}