#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/constants.hpp>

#include <type_traits>

namespace oblo
{
    constexpr bool float_equal(f32 lhs, f32 rhs, f32 tolerance = epsilon)
    {
        return lhs >= rhs - tolerance && lhs <= rhs + tolerance;
    }

    template <typename T>
        requires(std::is_trivially_copyable_v<T> && sizeof(T) % sizeof(f32) == 0)
    constexpr bool float_equal(const T& lhs, const T& rhs, f32 tolerance = epsilon)
    {
        constexpr std::size_t count = sizeof(T) / sizeof(f32);

        struct array
        {
            float f32[count];
        };

        const auto lhsF32 = reinterpret_cast<const array&>(lhs);
        const auto rhsF32 = reinterpret_cast<const array&>(rhs);

        for (std::size_t i = 0; i < count; ++i)
        {
            if (!float_equal(lhsF32.f32[i], rhsF32.f32[i], tolerance))
            {
                return false;
            }
        }

        return true;
    }
}