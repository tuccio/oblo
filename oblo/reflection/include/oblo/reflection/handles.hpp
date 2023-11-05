#pragma once

#include <oblo/core/handle.hpp>

namespace oblo::reflection
{
    struct class_data;
    struct type_data;

    struct type_handle
    {
        constexpr explicit operator bool() const noexcept
        {
            return value != u32{};
        }

        constexpr auto operator<=>(const type_handle&) const = default;

        u32 value;
    };

    struct class_handle
    {
        constexpr explicit operator bool() const noexcept
        {
            return value != u32{};
        }

        constexpr auto operator<=>(const class_handle&) const = default;

        u32 value;

        // Class handles are also type handles, conversion is implicit
        operator type_handle() const noexcept
        {
            return type_handle{value};
        }
    };
}