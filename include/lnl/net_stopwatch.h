#pragma once

#include <chrono>

namespace lnl {
    class net_stopwatch final {
        std::chrono::time_point<std::chrono::steady_clock> m_start;

        decltype(m_start)::duration m_last_time{};
        bool m_running = false;
    public:
        void start() {
            if (m_running) {
                return;
            }

            m_start = std::chrono::high_resolution_clock::now();
            m_running = true;
        }

        void stop() {
            if (!m_running) {
                return;
            }

            m_last_time = std::chrono::high_resolution_clock::now() - m_start;
            m_running = false;
        }

        void restart() {
            m_start = std::chrono::high_resolution_clock::now();
            m_running = true;
        }

        void reset() {
            m_start = std::chrono::high_resolution_clock::now();
        }

        [[maybe_unused]] int32_t milliseconds() const {
            if (!m_running) {
                return (int32_t) m_last_time.count();
            }

            return (int32_t) (std::chrono::high_resolution_clock::now() - m_start).count();
        }
    };
}