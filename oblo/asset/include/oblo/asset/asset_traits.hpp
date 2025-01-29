#pragma once

#include <oblo/core/uuid.hpp>

namespace oblo
{
    template <typename T>
    struct asset_traits;

    template <typename T>
    constexpr uuid asset_type = asset_traits<T>::uuid;
}
