#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    struct vec3i
    {
        i32 x;
        i32 y;
        i32 z;

        constexpr i32 dot(const vec3i& rhs) const noexcept
        {
            return x * rhs.x + y * rhs.y + z * rhs.z;
        }
    };

    constexpr vec3i operator-(const vec3i& lhs, const vec3i& rhs) noexcept
    {
        return {
            lhs.x - rhs.x,
            lhs.y - rhs.y,
            lhs.z - rhs.z,
        };
    }

    constexpr vec3i operator-(const vec3i& lhs, i32 rhs) noexcept
    {
        return {
            lhs.x - rhs,
            lhs.y - rhs,
            lhs.z - rhs,
        };
    }

    constexpr vec3i operator+(const vec3i& lhs, const vec3i& rhs) noexcept
    {
        return {
            lhs.x + rhs.x,
            lhs.y + rhs.y,
            lhs.z + rhs.z,
        };
    }

    constexpr vec3i operator*(const vec3i& lhs, const vec3i& rhs) noexcept
    {
        return {
            lhs.x * rhs.x,
            lhs.y * rhs.y,
            lhs.z * rhs.z,
        };
    }

    constexpr vec3i operator/(const vec3i& lhs, const vec3i& rhs) noexcept
    {
        return {
            lhs.x / rhs.x,
            lhs.y / rhs.y,
            lhs.z / rhs.z,
        };
    }

    constexpr vec3i operator/(const vec3i& lhs, i32 rhs) noexcept
    {
        return lhs / vec3i{rhs, rhs, rhs};
    }

    constexpr vec3i operator*(i32 lhs, const vec3i& rhs) noexcept
    {
        return vec3i{lhs, lhs, lhs} * rhs;
    }

    constexpr vec3i operator*(const vec3i& lhs, i32 rhs) noexcept
    {
        return rhs * lhs;
    }
}
