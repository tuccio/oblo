#pragma once

#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    template <typename T>
    struct resource_ref
    {
        uuid id;

        explicit operator bool() const noexcept
        {
            return !id.is_nil();
        }

        bool operator==(const resource_ref&) const = default;
    };

    template <typename T>
    struct hash<resource_ref<T>>
    {
        constexpr auto operator()(const resource_ref<T>& ref) const noexcept
        {
            return hash<uuid>{}(ref.id);
        }
    };
}