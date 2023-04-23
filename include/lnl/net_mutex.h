#pragma once

#ifdef WIN32
#include <Windows.h>
#elif __linux__

#include <pthread.h>

#endif

#include <atomic>

namespace lnl {
    class net_mutex final {
#ifdef WIN32
        CRITICAL_SECTION m_handle{};
#elif __linux__
        pthread_mutex_t m_handle{};
        pthread_mutexattr_t m_attribute{};
#endif
    public:
        net_mutex() {
#ifdef WIN32
            InitializeCriticalSection(&m_handle);
#elif __linux__
            pthread_mutexattr_init(&m_attribute);
            pthread_mutexattr_settype(&m_attribute, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&m_handle, &m_attribute);
#endif
        }

        ~net_mutex() {
#ifdef WIN32
            DeleteCriticalSection(&m_handle);
#elif __linux__
            pthread_mutex_destroy(&m_handle);
            pthread_mutexattr_destroy(&m_attribute);
#endif
            m_handle = {};
        }

        friend class net_mutex_guard;
    };

    class net_mutex_guard final {
#ifdef WIN32
        LPCRITICAL_SECTION m_cs;
#elif __linux__
        pthread_mutex_t* m_cs;
#endif
        std::atomic<bool> m_released = false;
    public:
        explicit net_mutex_guard(const net_mutex& mutex) {
            m_cs = const_cast<decltype(mutex.m_handle)*>(&mutex.m_handle);
#ifdef WIN32
            EnterCriticalSection(m_cs);
#elif __linux__
            pthread_mutex_lock(m_cs);
#endif
        }

        void release() {
            m_released = true;
#ifdef WIN32
            LeaveCriticalSection(m_cs);
#elif __linux__
            pthread_mutex_unlock(m_cs);
#endif
        }

        ~net_mutex_guard() {
            if (m_released) {
                return;
            }

            release();
        }
    };
}