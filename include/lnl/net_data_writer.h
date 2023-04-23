#pragma once

#include <vector>
#include <type_traits>

namespace lnl {
    template <bool cond, typename U>
    using resolvedType = typename std::enable_if<cond, U>::type;

    class net_data_writer final {
        static constexpr size_t GROWTH_FACTOR = 2;

        std::vector<uint8_t> m_data;
        size_t m_position = 0;
    public:
        [[nodiscard]] size_t size() const {
            return m_position;
        }

        [[nodiscard]] const uint8_t* data() const {
            return m_data.data();
        }

        [[nodiscard]] uint8_t* data() {
            return m_data.data();
        }

        void reset() {
            m_position = 0;
        }

        template <typename T>
        typename std::enable_if<
                std::is_fundamental<
                        typename std::remove_const<
                                typename std::remove_reference<T>::type>::type>::value>::type write(T&& value) {
            write((uint8_t*) &value, 0, sizeof(T));
        }

        template <typename T>
        inline void write(const std::vector<T>& vector) {
            write((uint16_t) vector.size());

            for (const auto& value: vector)
                write(value);
        }

        template <typename T>
        inline typename std::enable_if<std::is_enum<T>::value>::type write(T&& value) {
            typename std::underlying_type<T>::type* tmp = (decltype(tmp)) &value;
            write(*tmp);
        }

        void write(const std::string& str) {
            write((uint16_t) str.size());
            write((uint8_t*) str.data(), 0, str.size());
        }

        void write(const uint8_t* src, size_t srcOffset, size_t size) {
            if (size == 0) {
                return;
            }

            ensure(size);

            memcpy(&m_data[m_position], &src[srcOffset], size);

            m_position += size;
        }

    private:
        void ensure(size_t size) {
            if (m_data.size() > size) {
                return;
            }

            do {
                auto newSize = m_data.empty() ? 1 : m_data.size();
                m_data.resize(newSize * GROWTH_FACTOR, 0);
            } while (m_data.size() <= size);
        }
    };
}