#pragma once

#include <oblo/core/uuid.hpp>

namespace oblo
{
    template <typename>
    struct resource_traits;

    template <typename T>
    constexpr uuid resource_type = resource_traits<T>::uuid;
}