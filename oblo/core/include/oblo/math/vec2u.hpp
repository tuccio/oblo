#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    struct vec2u
    {
        u32 x;
        u32 y;

        constexpr u32 dot(const vec2u& rhs) const noexcept
        {
            return x * rhs.x + y * rhs.y;
        }
    };

    constexpr vec2u operator-(const vec2u& lhs, const vec2u& rhs) noexcept
    {
        return {
            lhs.x - rhs.x,
            lhs.y - rhs.y,
        };
    }

    constexpr vec2u operator-(const vec2u& lhs, u32 rhs) noexcept
    {
        return {
            lhs.x - rhs,
            lhs.y - rhs,
        };
    }

    constexpr vec2u operator+(const vec2u& lhs, const vec2u& rhs) noexcept
    {
        return {
            lhs.x + rhs.x,
            lhs.y + rhs.y,
        };
    }

    constexpr vec2u operator*(const vec2u& lhs, const vec2u& rhs) noexcept
    {
        return {
            lhs.x * rhs.x,
            lhs.y * rhs.y,
        };
    }

    constexpr vec2u operator/(const vec2u& lhs, const vec2u& rhs) noexcept
    {
        return {
            lhs.x / rhs.x,
            lhs.y / rhs.y,
        };
    }

    constexpr vec2u operator/(const vec2u& lhs, u32 rhs) noexcept
    {
        return lhs / vec2u{rhs, rhs};
    }

    constexpr vec2u operator*(u32 lhs, const vec2u& rhs) noexcept
    {
        return vec2u{lhs, lhs} * rhs;
    }

    constexpr vec2u operator*(const vec2u& lhs, u32 rhs) noexcept
    {
        return rhs * lhs;
    }
}