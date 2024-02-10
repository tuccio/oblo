#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/constants.hpp>

namespace oblo
{
    struct radians_tag
    {
    };

    struct degrees_tag
    {
    };

    constexpr f32 convert(f32 value, radians_tag, degrees_tag)
    {
        return value * 180.f / pi;
    }

    constexpr f32 convert(f32 value, degrees_tag, radians_tag)
    {
        return value * pi / 180.f;
    }

    template <typename Tag>
    struct angle
    {
        f32 value;

        angle() = default;
        angle(const angle&) = default;
        angle(angle&&) noexcept = default;

        constexpr explicit angle(f32 value) : value{value} {}

        angle& operator=(const angle&) = default;
        angle& operator=(angle&&) noexcept = default;

        ~angle() = default;

        template <typename TOther>
        constexpr angle(const angle<TOther>& other) : value{convert(f32(other), TOther{}, Tag{})}
        {
        }

        constexpr explicit operator f32() const
        {
            return value;
        }
    };

    using radians = angle<radians_tag>;
    using degrees = angle<degrees_tag>;

    template <typename Tag>
    angle<Tag> operator+(angle<Tag> lhs, angle<Tag> rhs)
    {
        return angle<Tag>{f32(lhs) + f32(rhs)};
    }

    template <typename Tag>
    angle<Tag> operator*(angle<Tag> lhs, f32 rhs)
    {
        return angle<Tag>{f32(lhs) * rhs};
    }

    template <typename Tag>
    angle<Tag> operator/(angle<Tag> lhs, f32 rhs)
    {
        return angle<Tag>{f32(lhs) / rhs};
    }

    inline namespace math_literals
    {
        constexpr radians operator""_rad(unsigned long long value)
        {
            return radians{f32(value)};
        }

        constexpr radians operator""_rad(long double value)
        {
            return radians{f32(value)};
        }

        constexpr degrees operator""_deg(unsigned long long value)
        {
            return degrees{f32(value)};
        }

        constexpr degrees operator""_deg(long double value)
        {
            return degrees{f32(value)};
        }
    }
}