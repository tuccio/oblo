#pragma once

#include <oblo/core/types.hpp>

#include <compare>

namespace oblo
{
    struct time
    {
        static constexpr time from_seconds(f32 seconds);
        static constexpr time from_milliseconds(u32 milliseconds);
        static constexpr time from_milliseconds(f32 milliseconds);

        // The unit is 100 nanoseconds (hns in short)
        i64 hns;

        constexpr auto operator<=>(const time&) const noexcept = default;
    };

    constexpr time time::from_seconds(f32 seconds)
    {
        return {i64(seconds * 1e7f)};
    }

    constexpr time time::from_milliseconds(f32 millis)
    {
        return {i64(millis * 1e4f)};
    }

    constexpr time time::from_milliseconds(u32 millis)
    {
        return {i64{millis} * 10'000};
    }

    constexpr time operator-(const time& lhs, const time& rhs)
    {
        return {lhs.hns - rhs.hns};
    }

    constexpr time operator+(const time& lhs, const time& rhs)
    {
        return {lhs.hns + rhs.hns};
    }

    constexpr f32 to_f32_seconds(time t)
    {
        return t.hns * 1e-7f;
    }
}