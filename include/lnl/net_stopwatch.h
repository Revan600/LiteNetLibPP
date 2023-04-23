#pragma once

#include <chrono>

namespace lnl {
    class net_stopwatch final {
        std::chrono::time_point<std::chrono::steady_clock> m_start;

        decltype(m_start)::duration m_last_time{};
        bool m_running = false;
    public:
        [[nodiscard]] bool running() const {
            return m_running;
        }

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

        [[nodiscard]] int32_t milliseconds() const {
            decltype(m_start)::duration duration;

            if (!m_running) {
                duration = m_last_time;
            } else {
                duration = std::chrono::high_resolution_clock::now() - m_start;
            }

            return (int32_t) std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        }
    };
}