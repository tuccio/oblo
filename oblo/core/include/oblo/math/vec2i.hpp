#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    struct vec2i
    {
        i32 x;
        i32 y;

        constexpr i32 dot(const vec2i& rhs) const noexcept
        {
            return x * rhs.x + y * rhs.y;
        }
    };

    constexpr vec2i operator-(const vec2i& lhs, const vec2i& rhs) noexcept
    {
        return {
            lhs.x - rhs.x,
            lhs.y - rhs.y,
        };
    }

    constexpr vec2i operator-(const vec2i& lhs, i32 rhs) noexcept
    {
        return {
            lhs.x - rhs,
            lhs.y - rhs,
        };
    }

    constexpr vec2i operator+(const vec2i& lhs, const vec2i& rhs) noexcept
    {
        return {
            lhs.x + rhs.x,
            lhs.y + rhs.y,
        };
    }

    constexpr vec2i operator*(const vec2i& lhs, const vec2i& rhs) noexcept
    {
        return {
            lhs.x * rhs.x,
            lhs.y * rhs.y,
        };
    }

    constexpr vec2i operator/(const vec2i& lhs, const vec2i& rhs) noexcept
    {
        return {
            lhs.x / rhs.x,
            lhs.y / rhs.y,
        };
    }

    constexpr vec2i operator/(const vec2i& lhs, i32 rhs) noexcept
    {
        return lhs / vec2i{rhs, rhs};
    }

    constexpr vec2i operator*(i32 lhs, const vec2i& rhs) noexcept
    {
        return vec2i{lhs, lhs} * rhs;
    }

    constexpr vec2i operator*(const vec2i& lhs, i32 rhs) noexcept
    {
        return rhs * lhs;
    }
}