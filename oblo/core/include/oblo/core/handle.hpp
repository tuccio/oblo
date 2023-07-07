#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename Tag>
    struct handle
    {
        constexpr explicit operator bool() const noexcept
        {
            return value != 0;
        }

        u32 value;
    };

    template <typename T>
    struct flat_key_extractor;

    template <typename Tag>
    struct flat_key_extractor<handle<Tag>>
    {
        static constexpr u32 extract_key(const handle<Tag> h) noexcept
        {
            return h.value;
        }

        static consteval u32 invalid_key() noexcept
        {
            return u32{};
        }
    };
}