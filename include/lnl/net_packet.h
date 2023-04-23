#pragma once

#include <cassert>
#include <cstdint>
#include <vector>
#include <type_traits>

#include <lnl/net_constants.h>
#include <lnl/net_enums.h>

namespace lnl {
    class net_packet final {
        static constexpr size_t GROWTH_FACTOR = 2;
        static constexpr std::array<size_t, static_cast<int32_t>(PACKET_PROPERTY::COUNT)> HEADER_SIZES = {
                net_constants::HEADER_SIZE, //UNRELIABLE
                net_constants::CHANNELED_HEADER_SIZE, //CHANNELED
                net_constants::CHANNELED_HEADER_SIZE, //ACK
                net_constants::HEADER_SIZE + 2, //PING
                net_constants::HEADER_SIZE + 10, //PONG
                net_constants::CONNECT_REQUEST_HEADER_SIZE, //CONNECT_REQUEST
                net_constants::CONNECT_ACCEPT_HEADER_SIZE, //CONNECT_ACCEPT
                net_constants::HEADER_SIZE + 8, //DISCONNECT
                net_constants::HEADER_SIZE, //MTU_CHECK
                net_constants::HEADER_SIZE, //MTU_OK
                net_constants::HEADER_SIZE, //BROADCAST
                net_constants::HEADER_SIZE, //MERGED
                net_constants::HEADER_SIZE, //SHUTDOWN_OK
                net_constants::HEADER_SIZE, //PEER_NOT_FOUND
                net_constants::HEADER_SIZE, //INVALID_PROTOCOL
                net_constants::HEADER_SIZE, //NAT_MESSAGE
                net_constants::HEADER_SIZE, //EMPTY
        };

        std::vector<uint8_t> m_data;
        size_t m_size = 0;
        net_packet* m_next = nullptr;
    public:
        void* user_data = nullptr;

        net_packet() {
            ensure(net_constants::MAX_PACKET_SIZE);
            m_size = net_constants::MAX_PACKET_SIZE;
        }

        net_packet(PACKET_PROPERTY property, size_t size) {
            size += net_packet::get_header_size(property);
            ensure(size);
            m_size = size;
            set_property(property);
        }

        void clear() {
            memset(m_data.data(), 0, m_data.size());
        }

        [[nodiscard]] const uint8_t* data() const {
            return m_data.data();
        }

        [[nodiscard]] size_t size() const {
            return m_size;
        }

        [[nodiscard]] size_t buffer_size() const {
            return m_data.size();
        }

        uint8_t* data() {
            assert(m_data.size() >= m_size);
            return m_data.data();
        }

        void resize(size_t size) {
            ensure(size);
            m_size = size;
        }

        void resize(PACKET_PROPERTY property, size_t size) {
            size += net_packet::get_header_size(property);
            resize(size);
            set_property(property);
        }

        template <typename T>
        T get_value_at(size_t position) const {
            return *(T*) &m_data[position];
        }

        template <typename T>
        void set_value_at(T&& value, size_t position) const {
            *(typename std::remove_const<typename std::remove_reference<T>::type>::type*) &m_data[position] = value;
        }

        void copy_from(const uint8_t* src, size_t srcOffset, size_t position, size_t size) {
            if (size == 0) {
                return;
            }

            memcpy(&m_data[position], &src[srcOffset], size);
        }

        [[nodiscard]] PACKET_PROPERTY property() const {
            return (PACKET_PROPERTY) (m_data[0] & 0x1F);
        }

        void set_property(PACKET_PROPERTY property) {
            m_data[0] = (uint8_t) ((m_data[0] & 0xE0) | (uint8_t) property);
        }

        [[nodiscard]] inline bool verify() const {
            auto property = (uint8_t) (m_data[0] & 0x1F);

            if (property >= static_cast<uint32_t>(PACKET_PROPERTY::COUNT)) {
                return false;
            }

            auto headerSize = HEADER_SIZES[property];
            bool fragmented = (m_data[0] & 0x80) != 0;
            return size() >= headerSize &&
                   (!fragmented || size() >= headerSize + net_constants::FRAGMENT_HEADER_SIZE);
        }

        static constexpr size_t get_header_size(PACKET_PROPERTY property) {
            return HEADER_SIZES[static_cast<int32_t>(property)];
        }

        [[nodiscard]] uint8_t connection_number() const {
            return (uint8_t) ((m_data[0] & 0x60) >> 5);
        }

        void set_connection_number(uint8_t value) {
            m_data[0] = (uint8_t) ((m_data[0] & 0x9F) | (value << 5));
        }

        [[nodiscard]]uint16_t sequence() const {
            return *(uint16_t*) &m_data[1];
        }

        void set_sequence(uint16_t value) {
            *(uint16_t*) &m_data[1] = value;
        }

        [[nodiscard]] uint8_t channel_id() const {
            return m_data[3];
        }

        void set_channel_id(uint8_t value) {
            m_data[3] = value;
        }

        [[nodiscard]] bool is_fragmented() const {
            return (m_data[0] & 0x80) != 0;
        }

        [[nodiscard]] uint16_t fragment_id() const {
            return *(uint16_t*) &m_data[4];
        }

        void set_fragment_id(uint16_t value) {
            *(uint16_t*) &m_data[4] = value;
        }

        [[nodiscard]] uint16_t fragment_part() const {
            return *(uint16_t*) &m_data[6];
        }

        void set_fragment_part(uint16_t value) {
            *(uint16_t*) &m_data[6] = value;
        }

        [[nodiscard]] uint16_t total_fragments() const {
            return *(uint16_t*) &m_data[8];
        }

        void set_total_fragments(uint16_t value) {
            *(uint16_t*) &m_data[8] = value;
        }

        void mark_fragmented() {
            m_data[0] |= 0x80;
        }

    private:
        void ensure(size_t size) {
            if (m_data.size() >= size) {
                return;
            }

            while (m_data.size() < size) {
                auto newSize = m_data.size() * GROWTH_FACTOR;

                if (newSize == 0) {
                    newSize = GROWTH_FACTOR;
                }

                m_data.resize(newSize, 0);
            }
        }

        friend class net_manager;
    };
}