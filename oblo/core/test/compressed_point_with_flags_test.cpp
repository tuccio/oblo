#include <gtest/gtest.h>

#include <oblo/core/compressed_pointer_with_flags.hpp>

namespace oblo
{
    TEST(compressed_pointer_with_flags, with_i32)
    {
        compressed_pointer_with_flags<i32> c{};

        static_assert(compressed_pointer_with_flags<i32>::max_flags == 2);
        EXPECT_EQ(compressed_pointer_with_flags<i32>::max_flags, 2);

        EXPECT_EQ(c.get_pointer(), nullptr);
        EXPECT_EQ(c.get_flag(0), false);
        EXPECT_EQ(c.get_flag(1), false);

        i32 value{42};

        c.set_pointer(&value);
        c.assign_flag(1, true);

        EXPECT_EQ(c.get_pointer(), &value);
        EXPECT_EQ(c.get_flag(0), false);
        EXPECT_EQ(c.get_flag(1), true);

        c.set_pointer(nullptr);

        EXPECT_EQ(c.get_pointer(), nullptr);
        EXPECT_EQ(c.get_flag(0), false);
        EXPECT_EQ(c.get_flag(1), true);

        c.assign_flag(0, true);
        c.assign_flag(1, false);

        EXPECT_EQ(c.get_pointer(), nullptr);
        EXPECT_EQ(c.get_flag(0), true);
        EXPECT_EQ(c.get_flag(1), false);

        c.set_pointer(&value);

        EXPECT_EQ(c.get_pointer(), &value);
        EXPECT_EQ(c.get_flag(0), true);
        EXPECT_EQ(c.get_flag(1), false);
    }
}