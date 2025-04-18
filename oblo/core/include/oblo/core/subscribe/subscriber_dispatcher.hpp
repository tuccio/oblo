#pragma once

#include <oblo/core/subscribe/subscriber_list.hpp>

namespace oblo
{
    template <typename Tag, typename Callback>
    class subscriber_dispatcher : public subscriber_list<Tag, Callback>
    {
    public:
        using subscriber_list_type = subscriber_list<Tag, Callback>;

    public:
        void dispatch() noexcept;

    private:
        using subscriber_list<Tag, Callback>::m_lock;
        using subscriber_list<Tag, Callback>::m_subscribers;
    };

    template <typename Tag, typename Callback>
    void subscriber_dispatcher<Tag, Callback>::dispatch() noexcept
    {
        OBLO_ASSERT(!m_lock);

        m_lock = true;

        for (auto& subscriber : m_subscribers.values())
        {
            subscriber.callback();
        }

        m_lock = false;
    }
}