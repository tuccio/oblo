#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    struct vec3u
    {
        u32 x;
        u32 y;
        u32 z;

        constexpr u32 dot(const vec3u& rhs) const noexcept
        {
            return x * rhs.x + y * rhs.y + z * rhs.z;
        }
    };

    constexpr vec3u operator-(const vec3u& lhs, const vec3u& rhs) noexcept
    {
        return {
            lhs.x - rhs.x,
            lhs.y - rhs.y,
            lhs.z - rhs.z,
        };
    }

    constexpr vec3u operator-(const vec3u& lhs, u32 rhs) noexcept
    {
        return {
            lhs.x - rhs,
            lhs.y - rhs,
            lhs.z - rhs,
        };
    }

    constexpr vec3u operator+(const vec3u& lhs, const vec3u& rhs) noexcept
    {
        return {
            lhs.x + rhs.x,
            lhs.y + rhs.y,
            lhs.z + rhs.z,
        };
    }

    constexpr vec3u operator*(const vec3u& lhs, const vec3u& rhs) noexcept
    {
        return {
            lhs.x * rhs.x,
            lhs.y * rhs.y,
            lhs.z * rhs.z,
        };
    }

    constexpr vec3u operator/(const vec3u& lhs, const vec3u& rhs) noexcept
    {
        return {
            lhs.x / rhs.x,
            lhs.y / rhs.y,
            lhs.z / rhs.z,
        };
    }

    constexpr vec3u operator/(const vec3u& lhs, u32 rhs) noexcept
    {
        return lhs / vec3u{rhs, rhs, rhs};
    }

    constexpr vec3u operator*(u32 lhs, const vec3u& rhs) noexcept
    {
        return vec3u{lhs, lhs, lhs} * rhs;
    }

    constexpr vec3u operator*(const vec3u& lhs, u32 rhs) noexcept
    {
        return rhs * lhs;
    }
}
