#include <gtest/gtest.h>

#include <oblo/core/string/string_builder.hpp>

namespace oblo
{
    TEST(string_builder, default_constructor)
    {
        string_builder sb;
        EXPECT_EQ(sb.size(), 0);
        EXPECT_STREQ(sb.data(), "");
    }

    TEST(string_builder, append_string_view)
    {
        string_builder sb;
        sb.append("Hello, ");
        sb.append("world!");
        EXPECT_EQ(sb.size(), 13);
        EXPECT_STREQ(sb.data(), "Hello, world!");
    }

    TEST(string_builder, append_format_string)
    {
        string_builder sb;
        sb.format("The number is: {}", 42);

        const char* expected = "The number is: 42";
        EXPECT_EQ(sb.size(), strlen(expected));
        EXPECT_STREQ(sb.data(), expected);
    }

    TEST(string_builder, join_format_string)
    {
        int values[] = {1, 2, 3, 4};

        string_builder sb;
        sb.join(std::begin(values), std::end(values), ", ");

        const char* expected = "1, 2, 3, 4";
        EXPECT_EQ(sb.size(), strlen(expected));
        EXPECT_STREQ(sb.data(), expected);
    }

    TEST(string_builder, clear)
    {
        string_builder sb;
        sb.append("Some content");
        sb.clear();
        EXPECT_EQ(sb.size(), 0);
        EXPECT_STREQ(sb.data(), "");
    }

    TEST(string_builder, view)
    {
        string_builder sb;
        sb.append("String view test");
        std::string_view view = sb.view();
        EXPECT_EQ(view.size(), 16);
        EXPECT_EQ(view, "String view test");
    }

    TEST(string_builder, conversion_to_string_view)
    {
        string_builder sb;
        sb.append("Conversion test");
        std::string_view view = static_cast<std::string_view>(sb);
        EXPECT_EQ(view.size(), 15);
        EXPECT_EQ(view, "Conversion test");
    }

    TEST(string_builder, assign_string_view)
    {
        string_builder sb;
        sb = "Assigned string";
        EXPECT_EQ(sb.size(), 15);
        EXPECT_STREQ(sb.data(), "Assigned string");
    }
}