#pragma once

namespace oblo::cppgenlib
{
    using f32 = float;
    using f64 = double;
    using i32 = int;
    using i64 = long long;
    using u32 = unsigned;
    using u64 = unsigned long long;

    static_assert(sizeof(f32) == 4u);
    static_assert(sizeof(f64) == 8u);
    static_assert(sizeof(i32) == 4u);
    static_assert(sizeof(i64) == 8u);
    static_assert(sizeof(u32) == 4u);
    static_assert(sizeof(u64) == 8u);

    namespace
    {
        constexpr f32 pi = 3.14159265358979323846f;
        constexpr f32 half_pi = 1.57079632679489661923f;
        constexpr f32 two_pi = 6.28318530717958647692f;
        constexpr f32 inv_two_pi = 0.15915494309189533577f;

        constexpr i32 reduce_quadrant(f32& x)
        {
            const i32 k = i32(x * (2.0f / pi));
            x = x - k * half_pi;
            return k & 3;
        }

        constexpr f32 sin_poly(f32 x)
        {
            const f32 x2 = x * x;
            return x * (1.0f + x2 * (-0.1666666716f + x2 * (0.0083333477f + x2 * (-0.0001984090f))));
        }

        [[noreturn]] void unreachable()
        {
#if defined(__GNUC__) || defined(__clang__)
            __builtin_unreachable();
#elif defined(_MSC_VER) // MSVC
            __assume(false);
#endif
        }
    }

    inline f32 sin_approx(f32 x)
    {
        // coarse reduction
        x -= i32(x * inv_two_pi) * two_pi;

        // quadrant reduction
        const i32 q = reduce_quadrant(x);

        switch (q)
        {
        case 0:
            return sin_poly(x);
        case 1:
            return sin_poly(half_pi - x);
        case 2:
            return -sin_poly(x);
        case 3:
            return -sin_poly(half_pi - x);
        default:
            unreachable();
        }
    }
}