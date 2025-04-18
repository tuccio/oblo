#pragma once

#include <oblo/core/subscribe/subscriber_dispatcher.hpp>
#include <oblo/core/subscribe/subscriber_list.hpp>

#include <functional>

namespace oblo::editor
{
    struct update_subscriber;

    using update_dispatcher = subscriber_dispatcher<update_subscriber, std::function<void()>>;
    using update_subscriptions = update_dispatcher::subscriber_list_type;
}