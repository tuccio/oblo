#include <gtest/gtest.h>

#include <oblo/core/span.hpp>

#include <array>
#include <initializer_list>

namespace oblo
{
    TEST(span, span_default_constructed)
    {
        span<i32> s;

        ASSERT_EQ(s.data(), nullptr);
        ASSERT_EQ(s.size(), 0);
        ASSERT_EQ(s.size32(), 0);
        ASSERT_TRUE(s.empty());
        ASSERT_EQ(s.begin(), s.end());
    }

    TEST(span, span_pointer_and_size)
    {
        i32 values[] = {1, 2, 3, 4, 5};

        span<i32> s{values, 5};

        ASSERT_EQ(s.data(), values);
        ASSERT_EQ(s.size(), 5);
        ASSERT_EQ(s.size32(), 5);
        ASSERT_FALSE(s.empty());

        ASSERT_EQ(s.begin(), values);
        ASSERT_EQ(s.end(), values + 5);
    }

    TEST(span, span_pointer_range)
    {
        i32 values[] = {1, 2, 3, 4, 5};

        span<i32> s{std::begin(values), std::end(values)};

        ASSERT_EQ(s.size(), 5);
        ASSERT_EQ(s.data(), values);
        ASSERT_EQ(s.end(), values + 5);
    }

    TEST(span, span_element_access)
    {
        i32 values[] = {10, 20, 30, 40, 50};

        span<i32> s{values, std::size(values)};

        ASSERT_EQ(s[0], 10);
        ASSERT_EQ(s[1], 20);
        ASSERT_EQ(s[2], 30);
        ASSERT_EQ(s[3], 40);
        ASSERT_EQ(s[4], 50);

        ASSERT_EQ(s.front(), 10);
        ASSERT_EQ(s.back(), 50);
    }

    TEST(span, span_mutable_access)
    {
        i32 values[] = {1, 2, 3};

        span<i32> s{values, std::size(values)};

        s[0] = 10;
        s.front() = 20;
        s.back() = 30;

        ASSERT_EQ(values[0], 20);
        ASSERT_EQ(values[1], 2);
        ASSERT_EQ(values[2], 30);

        for (auto& value : s)
        {
            value *= 2;
        }

        ASSERT_EQ(values[0], 40);
        ASSERT_EQ(values[1], 4);
        ASSERT_EQ(values[2], 60);
    }

    TEST(span, span_const_element_type)
    {
        const i32 values[] = {1, 2, 3};

        span<const i32> s{values, std::size(values)};

        ASSERT_EQ(s.size(), 3);
        ASSERT_EQ(s[0], 1);
        ASSERT_EQ(s.front(), 1);
        ASSERT_EQ(s.back(), 3);

        static_assert(std::is_same_v<decltype(s[0]), const i32&>);
        static_assert(std::is_same_v<decltype(s.data()), const i32*>);
    }

    TEST(span, span_iteration)
    {
        constexpr i32 values[] = {1, 2, 3, 4, 5};

        span<const i32> s{std::begin(values), std::end(values)};

        i32 expected = 1;

        for (const auto value : s)
        {
            ASSERT_EQ(value, expected++);
        }

        ASSERT_EQ(expected, 6);
    }

    TEST(span, span_empty_range)
    {
        i32 values[] = {1, 2, 3};

        span<i32> s{values, values};

        ASSERT_EQ(s.data(), values);
        ASSERT_EQ(s.size(), 0);
        ASSERT_TRUE(s.empty());
        ASSERT_EQ(s.begin(), s.end());
    }

    TEST(span, span_size32)
    {
        i32 values[] = {1, 2, 3, 4};

        span<i32> s{values, std::size(values)};

        ASSERT_EQ(s.size32(), 4u);
    }

    TEST(span, span_make_initializer)
    {
        const auto s = make_span_initializer<i32>({1, 2, 3, 4});

        ASSERT_EQ(s.size(), 4);
        ASSERT_EQ(s[0], 1);
        ASSERT_EQ(s[1], 2);
        ASSERT_EQ(s[2], 3);
        ASSERT_EQ(s[3], 4);
    }

    TEST(span, span_make_initializer_empty)
    {
        const auto s = make_span_initializer<i32>({});

        ASSERT_EQ(s.size(), 0);
        ASSERT_TRUE(s.empty());
    }

    TEST(span, span_array)
    {
        std::array<i32, 3> values{10, 20, 30};

        span<i32> s{values.data(), values.size()};

        ASSERT_EQ(s.size(), values.size());
        ASSERT_TRUE(std::equal(s.begin(), s.end(), values.begin()));
    }
}