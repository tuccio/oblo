#pragma once

#include <cmath>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

namespace oblo
{
    struct vec3
    {
        f32 x;
        f32 y;
        f32 z;

        constexpr f32& operator[](u32 index)
        {
            return *(&x + index);
        }

        constexpr const f32& operator[](u32 index) const
        {
            return *(&x + index);
        }

        inline vec3& operator+=(const vec3& rhs) noexcept;
    };

    constexpr vec3 operator-(const vec3& lhs) noexcept
    {
        return {
            -lhs.x,
            -lhs.y,
            -lhs.z,
        };
    }

    constexpr bool operator==(const vec3& lhs, const vec3& rhs) noexcept
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    constexpr bool operator!=(const vec3& lhs, const vec3& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    constexpr vec3 operator-(const vec3& lhs, const vec3& rhs) noexcept
    {
        return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }

    constexpr vec3 operator+(const vec3& lhs, const vec3& rhs) noexcept
    {
        return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    }

    constexpr vec3 operator+(f32 lhs, const vec3& rhs) noexcept
    {
        return {lhs + rhs.x, lhs + rhs.y, lhs + rhs.z};
    }

    constexpr vec3 operator+(const vec3& lhs, f32 rhs) noexcept
    {
        return {lhs.x + rhs, lhs.y + rhs, lhs.z + rhs};
    }

    constexpr vec3 operator*(const vec3& lhs, const vec3& rhs) noexcept
    {
        return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
    }

    constexpr vec3 operator/(const vec3& lhs, const vec3& rhs) noexcept
    {
        return {lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z};
    }

    constexpr vec3 operator/(f32 lhs, const vec3& rhs) noexcept
    {
        return vec3{lhs, lhs, lhs} / rhs;
    }

    constexpr vec3 operator/(const vec3& lhs, f32 rhs) noexcept
    {
        return lhs / vec3{rhs, rhs, rhs};
    }

    constexpr vec3 operator*(const vec3& lhs, f32 rhs) noexcept
    {
        return lhs * vec3{rhs, rhs, rhs};
    }

    constexpr vec3 operator*(f32 lhs, const vec3& rhs) noexcept
    {
        return rhs * lhs;
    }

    constexpr f32 dot(const vec3& lhs, const vec3& rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    constexpr vec3 cross(const vec3& lhs, const vec3& rhs) noexcept
    {
        return {lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x};
    }

    constexpr float length2(const vec3& v)
    {
        return dot(v, v);
    }

    inline float length(const vec3& v) noexcept
    {
        return std::sqrt(dot(v, v));
    }

    inline vec3 normalize(const vec3& v) noexcept
    {
        return v / length(v);
    }

    inline vec3& vec3::operator+=(const vec3& rhs) noexcept
    {
        return *this = *this + rhs;
    }

    template <>
    constexpr vec3 min<vec3>(const vec3 lhs, const vec3 rhs) noexcept
    {
        return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z)};
    }

    template <>
    constexpr vec3 max<vec3>(const vec3 lhs, const vec3 rhs) noexcept
    {
        return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z)};
    }

    constexpr vec3 lerp(const vec3& lhs, const vec3& rhs, f32 alpha)
    {
        return alpha * lhs + (1.f - alpha) * rhs;
    }
}