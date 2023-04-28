#pragma once

#include <unordered_map>
#include <functional>

namespace lnl {
    template <typename... argsType>
    class net_delegate final {
        typedef typename std::unordered_map<size_t,
                std::function<void(argsType...)>> subscriptions_t;

        size_t m_id_counter = 0;

        subscriptions_t m_subscriptions;
    public:
        size_t subscribe(typename subscriptions_t::value_type::second_type handler) {
            auto id = m_id_counter++;
            m_subscriptions.emplace(id, std::move(handler));
            return id;
        }

        bool unsubscribe(size_t id) {
            return m_subscriptions.erase(id) > 0;
        }

        void operator()(argsType... args) {
            for (auto& func: m_subscriptions) {
                func.second(args...);
            }
        }
    };
}