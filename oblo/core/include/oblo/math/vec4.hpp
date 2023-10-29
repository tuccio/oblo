#pragma once

#include <cmath>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

namespace oblo
{
    struct vec4
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;

        constexpr f32& operator[](u32 index)
        {
            return *(&x + index);
        }

        constexpr const f32& operator[](u32 index) const
        {
            return *(&x + index);
        }

        inline vec4& operator+=(const vec4& rhs) noexcept;
    };

    constexpr vec4 operator-(const vec4& lhs) noexcept
    {
        return {
            -lhs.x,
            -lhs.y,
            -lhs.z,
            -lhs.w,
        };
    }

    constexpr bool operator==(const vec4& lhs, const vec4& rhs) noexcept
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
    }

    constexpr bool operator!=(const vec4& lhs, const vec4& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    constexpr vec4 operator-(const vec4& lhs, const vec4& rhs) noexcept
    {
        return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
    }

    constexpr vec4 operator+(const vec4& lhs, const vec4& rhs) noexcept
    {
        return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
    }

    constexpr vec4 operator+(f32 lhs, const vec4& rhs) noexcept
    {
        return {lhs + rhs.x, lhs + rhs.y, lhs + rhs.z, lhs + rhs.w};
    }

    constexpr vec4 operator+(const vec4& lhs, f32 rhs) noexcept
    {
        return {lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs};
    }

    constexpr vec4 operator*(const vec4& lhs, const vec4& rhs) noexcept
    {
        return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w};
    }

    constexpr vec4 operator/(const vec4& lhs, const vec4& rhs) noexcept
    {
        return {lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w};
    }

    constexpr vec4 operator/(f32 lhs, const vec4& rhs) noexcept
    {
        return vec4{lhs, lhs, lhs, lhs} / rhs;
    }

    constexpr vec4 operator/(const vec4& lhs, f32 rhs) noexcept
    {
        return lhs / vec4{rhs, rhs, rhs, rhs};
    }

    constexpr vec4 operator*(const vec4& lhs, f32 rhs) noexcept
    {
        return lhs * vec4{rhs, rhs, rhs, rhs};
    }

    constexpr vec4 operator*(f32 lhs, const vec4& rhs) noexcept
    {
        return rhs * lhs;
    }

    constexpr f32 dot(const vec4& lhs, const vec4& rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
    }

    constexpr float length2(const vec4& v)
    {
        return dot(v, v);
    }

    inline float length(const vec4& v) noexcept
    {
        return std::sqrt(dot(v, v));
    }

    inline vec4 normalize(const vec4& v) noexcept
    {
        return v / length(v);
    }

    inline vec4& vec4::operator+=(const vec4& rhs) noexcept
    {
        return *this = *this + rhs;
    }

    template <>
    constexpr vec4 min<vec4>(const vec4 lhs, const vec4 rhs) noexcept
    {
        return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w)};
    }

    template <>
    constexpr vec4 max<vec4>(const vec4 lhs, const vec4 rhs) noexcept
    {
        return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w)};
    }

    constexpr vec4 lerp(const vec4& lhs, const vec4& rhs, f32 alpha)
    {
        return alpha * lhs + (1.f - alpha) * rhs;
    }
}