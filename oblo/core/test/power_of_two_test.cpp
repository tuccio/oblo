#include <gtest/gtest.h>

#include <oblo/core/types.hpp>
#include <oblo/math/power_of_two.hpp>

namespace oblo
{
    TEST(power_of_two, is_power_of_two)
    {
        for (u32 i = 0; i < (1u << 16); ++i)
        {
            const bool isPowerOfTwo = std::popcount(i) == 1;
            ASSERT_EQ(is_power_of_two(i), isPowerOfTwo);
            ASSERT_EQ(is_power_of_two(u64(i)), isPowerOfTwo);
            ASSERT_EQ(is_power_of_two(u64(i) << 32), isPowerOfTwo);
        }
    }

    TEST(power_of_two, round_up)
    {
        const auto bruteForceRoundUp = [](auto x)
        {
            u32 power = 1;

            while (power < x)
            {
                power *= 2;
            }

            return power;
        };

        for (u32 i = 0; i < (1u << 16); ++i)
        {
            const auto expected = bruteForceRoundUp(i);
            ASSERT_EQ(expected, round_up_power_of_two(i));
        }
    }
}