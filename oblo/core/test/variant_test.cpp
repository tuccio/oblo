#include <oblo/core/overload.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/variant.hpp>

#include <gtest/gtest.h>

namespace oblo
{
    TEST(variant, basic_storage)
    {
        variant<int, string> v{42};
        EXPECT_TRUE(v.is<int>());
        EXPECT_EQ(v.as<int>(), 42);

        v = string("hello");
        EXPECT_TRUE(v.is<string>());
        EXPECT_EQ(v.as<string>(), "hello");
    }

    TEST(variant, visitor)
    {
        const variant<int, string> v{string("test")};

        const auto result = visit(
            overload{
                [](int) -> string_view { return "int"; },
                [](const string&) -> string_view { return "string"; },
            },
            v);

        EXPECT_EQ(result, "string");
    }

    TEST(variant, emplace)
    {
        variant<int, string> v;
        v.emplace<string>("emplaced");

        EXPECT_TRUE(v.is<string>());
        EXPECT_EQ(v.as<string>(), "emplaced");
    }

    TEST(variant, default_holds_first)
    {
        variant<int, string> v;
        EXPECT_TRUE(v.is<int>());
        EXPECT_EQ(v.as<int>(), 0);
    }
}
