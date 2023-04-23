#pragma once

#include <lnl/net_mutex.h>

#include <queue>
#include <optional>

namespace lnl {
    //super dumb thread safe queue
    //todo: optimize
    template <typename T>
    class net_queue final {
        std::queue<T> m_queue;
        net_mutex m_mutex;
    public:
        void push(T& item) {
            net_mutex_guard guard(m_mutex);
            m_queue.push(item);
        }

        [[nodiscard]] bool empty() const {
            net_mutex_guard guard(m_mutex);
            return m_queue.empty();
        }

        [[nodiscard]] size_t size() const {
            return m_queue.size();
        }

        std::optional<T> dequeue() {
            net_mutex_guard guard(m_mutex);

            if (m_queue.empty()) {
                return {};
            }

            std::optional<T> result(m_queue.front());
            m_queue.pop();
            return result;
        }
    };
}