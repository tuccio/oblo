#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>

#include <functional>

namespace oblo::editor
{
    class update_dispatcher;
    struct update_subscriber;

    class update_subscriptions
    {
    public:
        using update_fn = std::function<void()>;

    public:
        update_subscriptions();
        update_subscriptions(const update_subscriptions&) = delete;
        update_subscriptions(update_subscriptions&&) noexcept = delete;
        update_subscriptions& operator=(const update_subscriptions&) = delete;
        update_subscriptions& operator=(update_subscriptions&&) noexcept = delete;
        ~update_subscriptions();

        h32<update_subscriber> subscribe(update_fn f);
        void unsubscribe(h32<update_subscriber> h);

    protected:
        h32_flat_pool_dense_map<update_subscriber> m_subscribers;
        bool m_lock{};
    };

    class update_dispatcher : public update_subscriptions
    {
    public:
        void update() noexcept;
    };
}