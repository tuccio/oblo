#include <oblo/editor/services/update_dispatcher.hpp>

namespace oblo::editor
{
    struct update_subscriber
    {
        update_subscriptions::update_fn callback;
    };

    update_subscriptions::update_subscriptions() = default;
    update_subscriptions::~update_subscriptions() = default;

    h32<update_subscriber> update_subscriptions::subscribe(update_fn f)
    {
        OBLO_ASSERT(!m_lock);
        const auto [it, h] = m_subscribers.emplace(std::move(f));
        return h;
    }

    void update_subscriptions::unsubscribe(h32<update_subscriber> h)
    {
        OBLO_ASSERT(!m_lock);
        m_subscribers.erase(h);
    }

    void update_dispatcher::update() noexcept
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