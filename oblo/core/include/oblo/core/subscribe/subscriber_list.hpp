#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>

namespace oblo
{
    template <typename Tag, typename Callback>
    struct subscriber
    {
        Callback callback;
    };

    template <typename Tag, typename Callback>
    class subscriber_list
    {
    public:
        using callback_fn = Callback;
        using subscriber_type = subscriber<Tag, Callback>;
        using subscriber_handle_type = h32<Tag>;

    public:
        subscriber_list() = default;
        subscriber_list(const subscriber_list&) = delete;
        subscriber_list(subscriber_list&&) noexcept = default;
        subscriber_list& operator=(const subscriber_list&) = delete;
        subscriber_list& operator=(subscriber_list&&) noexcept = default;
        ~subscriber_list() = default;

        subscriber_handle_type subscribe(callback_fn f);
        void unsubscribe(subscriber_handle_type h);

    protected:
        h32_flat_pool_dense_map<Tag, subscriber_type> m_subscribers;
        bool m_lock{};
    };

    template <typename Tag, typename Callback>
    subscriber_list<Tag, Callback>::subscriber_handle_type subscriber_list<Tag, Callback>::subscribe(callback_fn f)
    {
        OBLO_ASSERT(!m_lock);
        const auto [it, h] = m_subscribers.emplace(std::move(f));
        return h;
    }

    template <typename Tag, typename Callback>
    void subscriber_list<Tag, Callback>::unsubscribe(subscriber_handle_type h)
    {
        OBLO_ASSERT(!m_lock);
        m_subscribers.erase(h);
    }
}