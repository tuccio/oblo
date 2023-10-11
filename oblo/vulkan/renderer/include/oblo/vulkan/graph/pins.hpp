#pragma once

#include <oblo/core/handle.hpp>

namespace oblo::vk
{
    template <typename T>
    struct resource_pin
    {
    };

    template <typename T>
    struct data_pin
    {
    };

    template <typename T>
    using resource = h32<resource_pin<T>>;

    template <typename T>
    using data = h32<data_pin<T>>;
}