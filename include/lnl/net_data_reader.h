#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

namespace lnl {
    class net_data_reader final {
        uint8_t* m_data;
        size_t m_size;
        size_t m_position;
    public:
        net_data_reader(uint8_t* data, size_t size, size_t position = 0) : m_data(data), m_size(size),
                                                                           m_position(position) {}

        template <typename T>
        inline typename std::enable_if<std::is_fundamental<T>::value, bool>::type try_read(T& result) {
            return try_read((uint8_t*) &result, sizeof(T));
        }

        bool try_read(std::string& result) {
            uint16_t sz;

            if (!try_read(sz)) {
                return false;
            }

            result.resize(sz);
            return try_read((uint8_t*) result.data(), sz);
        }

        bool try_read(uint8_t* buffer, size_t size) {
            if (!check_boundaries(size)) {
                return false;
            }

            memcpy(buffer, &m_data[m_position], size);
            m_position += size;

            return true;
        }

    private:
        [[nodiscard]] bool check_boundaries(size_t size) const {
            return m_size - m_position >= size;
        }
    };
}