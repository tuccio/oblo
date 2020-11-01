#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    struct vec2
    {
        f32 x;
        f32 y;

        constexpr f32 dot(const vec2& rhs) const noexcept
        {
            return x * rhs.x + y * rhs.y;
        }
    };

    constexpr vec2 operator-(const vec2& lhs) noexcept
    {
        return {-lhs.x, -lhs.y};
    }

    constexpr vec2 operator-(const vec2& lhs, const vec2& rhs) noexcept
    {
        return {
            lhs.x - rhs.x,
            lhs.y - rhs.y,
        };
    }

    constexpr vec2 operator-(const vec2& lhs, f32 rhs) noexcept
    {
        return {
            lhs.x - rhs,
            lhs.y - rhs,
        };
    }

    constexpr vec2 operator+(const vec2& lhs, const vec2& rhs) noexcept
    {
        return {
            lhs.x + rhs.x,
            lhs.y + rhs.y,
        };
    }

    constexpr vec2 operator*(const vec2& lhs, const vec2& rhs) noexcept
    {
        return {
            lhs.x * rhs.x,
            lhs.y * rhs.y,
        };
    }

    constexpr vec2 operator/(const vec2& lhs, const vec2& rhs) noexcept
    {
        return {
            lhs.x / rhs.x,
            lhs.y / rhs.y,
        };
    }

    constexpr vec2 operator/(const vec2& lhs, f32 rhs) noexcept
    {
        return lhs / vec2{rhs, rhs};
    }

    constexpr vec2 operator*(f32 lhs, const vec2& rhs) noexcept
    {
        return vec2{lhs, lhs} * rhs;
    }

    constexpr vec2 operator*(const vec2& lhs, f32 rhs) noexcept
    {
        return rhs * lhs;
    }
}