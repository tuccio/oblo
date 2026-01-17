#include <cppgenlib.hpp>

#include <gtest/gtest.h>

#include <oblo/math/constants.hpp>

#include <cmath>

namespace oblo
{
    TEST(cppgenlib_math, sin)
    {
        EXPECT_NEAR(cppgenlib::sin(0.f), 0.f, 1e-3f);
        EXPECT_NEAR(cppgenlib::sin(pi / 2.f), 1.f, 1e-3f);
        EXPECT_NEAR(cppgenlib::sin(pi), 0.f, 1e-3f);
        EXPECT_NEAR(cppgenlib::sin(-pi / 2.f), -1.f, 1e-3f);
    }

    // TEST(cppgenlib_math, sin_symmetry)
    //{
    //     for (float x = -3.f; x <= 3.f; x += 0.1f)
    //     {
    //         EXPECT_NEAR(cppgenlib::sin(-x), -cppgenlib::sin(x), 1e-6f);
    //     }
    // }

    // TEST(cppgenlib_math, sin_periodicity)
    //{
    //     const float TWO_PI = 6.283185307f;

    //    for (float x = -3.f; x <= 3.f; x += 0.1f)
    //    {
    //        EXPECT_NEAR(cppgenlib::sin(x), cppgenlib::sin(x + TWO_PI), 1e-6f);
    //    }
    //}

    // TEST(cppgenlib_math, sin_matches_stdlib)
    //{
    //     for (float x = -6.28f; x <= 6.28f; x += 0.01f)
    //     {
    //         float expected = std::sinf(x);
    //         float actual = cppgenlib::sin(x);

    //        EXPECT_NEAR(actual, expected, 1e-6f) << "x = " << x;
    //    }
    //}

    // TEST(cppgenlib_math, sin_large_values)
    //{
    //     EXPECT_NEAR(cppgenlib::sin(1000.f), std::sinf(1000.f), 1e-6f);
    //     EXPECT_NEAR(cppgenlib::sin(-1000.f), std::sinf(-1000.f), 1e-6f);
    //     EXPECT_NEAR(cppgenlib::sin(1e6f), std::sinf(1e6f), 1e-6f);
    // }

    // TEST(cppgenlib_math, sin_continuity)
    //{
    //     float prev = cppgenlib::sin(-3.14f);

    //    for (float x = -3.14f; x <= 3.14f; x += 0.001f)
    //    {
    //        float curr = cppgenlib::sin(x);
    //        EXPECT_LT(std::fabs(curr - prev), 0.01f);
    //        prev = curr;
    //    }
    //}
}