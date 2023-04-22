#pragma once

#include <Windows.h>
#include <atomic>

namespace lnl {
    class net_mutex final {
        CRITICAL_SECTION m_handle{};
    public:
        net_mutex() {
            InitializeCriticalSection(&m_handle);
        }

        ~net_mutex() {
            DeleteCriticalSection(&m_handle);
            m_handle = {};
        }

        friend class net_mutex_guard;
    };

    class net_mutex_guard final {
        LPCRITICAL_SECTION m_cs;
        std::atomic<bool> m_released = false;
    public:
        explicit net_mutex_guard(const net_mutex& mutex) {
            m_cs = const_cast<LPCRITICAL_SECTION>(&mutex.m_handle);
            EnterCriticalSection(m_cs);
        }

        void release() {
            m_released = true;
            LeaveCriticalSection(m_cs);
        }

        ~net_mutex_guard() {
            if (m_released) {
                return;
            }

            m_released = true;
            LeaveCriticalSection(m_cs);
        }
    };
}