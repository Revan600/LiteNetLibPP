#pragma once

#include <lnl/net_mutex.h>
#include <queue>

namespace lnl {
    //super dumb thread safe queue
    //todo: optimize
    template <typename T>
    class net_queue final {
        std::queue<T> m_queue;
        net_mutex m_mutex;
    public:
        void push(T&& item) {
            net_mutex_guard guard(m_mutex);
            m_queue.push(item);
        }

        T& front() {
            net_mutex_guard guard(m_mutex);
            return m_queue.front();
        }

        void pop() {
            net_mutex_guard guard(m_mutex);
            m_queue.pop();
        }
    };
}