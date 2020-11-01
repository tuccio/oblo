#pragma once

#include <oblo/math/constants.hpp>

namespace oblo
{
    struct radians_tag
    {
    };

    struct degrees_tag
    {
    };

    template <typename Tag>
    class angle
    {
        static constexpr f32 convert(f32 value, radians_tag, degrees_tag)
        {
            return value * 180.f / pi;
        }

        static constexpr f32 convert(f32 value, degrees_tag, radians_tag)
        {
            return value * pi / 180.f;
        }

    public:
        constexpr angle() = default;
        constexpr angle(const angle&) = default;
        constexpr angle(angle&&) noexcept = default;

        angle& operator=(const angle&) = default;
        angle& operator=(angle&&) noexcept = default;

        constexpr explicit angle(f32 value) : m_value{value} {}

        template <typename TOther>
        constexpr angle(const angle<TOther>& angle) : m_value{convert(f32(angle), TOther{}, Tag{})}
        {
        }

        constexpr explicit operator f32() const
        {
            return m_value;
        }

    private:
        f32 m_value;
    };

    using radians = angle<radians_tag>;
    using degrees = angle<degrees_tag>;

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