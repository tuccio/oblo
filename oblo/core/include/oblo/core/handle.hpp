#pragma once

#include <oblo/core/types.hpp>

#include <compare>

namespace oblo
{
    template <typename Tag, typename Value>
    struct handle
    {
        using tag_type = Tag;
        using value_type = Value;

        constexpr explicit operator bool() const noexcept
        {
            return value != Value{};
        }

        constexpr auto operator<=>(const handle&) const = default;

        Value value;
    };

    template <typename T>
    struct flat_key_extractor;

    template <typename Tag, typename Value>
    struct flat_key_extractor<handle<Tag, Value>>
    {
        static constexpr Value extract_key(const handle<Tag, Value> h) noexcept
        {
            return h.value;
        }

        static consteval Value invalid_key() noexcept
        {
            return u32{};
        }
    };

    template <typename Tag>
    using h8 = handle<Tag, u8>;

    template <typename Tag>
    using h16 = handle<Tag, u16>;

    template <typename Tag>
    using h32 = handle<Tag, u32>;

    template <typename Tag>
    using h64 = handle<Tag, u64>;

    template <typename Tag>
    using hptr = handle<Tag, uintptr>;
}