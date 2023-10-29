#pragma once

#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    template <typename T>
    struct asset_ref
    {
        uuid id;

        explicit operator bool() const noexcept
        {
            return !id.is_nil();
        }
    };
}