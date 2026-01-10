#include <gtest/gtest.h>

#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/string_builder.hpp>

namespace oblo
{
    TEST(platform, read_write_env_variable)
    {
        constexpr auto key = u8"OBLO_DUMMY_VAR🚀";
        constexpr auto value = u8"Beaver 🦫";

        string_builder buffer;

        ASSERT_FALSE(platform::read_environment_variable(buffer, key));

        ASSERT_TRUE(platform::write_environment_variable(key, value));

        ASSERT_TRUE(platform::read_environment_variable(buffer, key));
        ASSERT_EQ(buffer, string_view{value});
    }
}