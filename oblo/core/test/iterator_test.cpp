#include <gtest/gtest.h>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/iterator/flags_range.hpp>
#include <oblo/core/iterator/reverse_iterator.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>

#include <array>
#include <span>
#include <vector>

namespace oblo
{
    namespace
    {
        struct array_test_data
        {
            static constexpr auto N = 42;
            static constexpr int K = 1337;

            int a1[N];
            int a2[N];

            static array_test_data setup()
            {
                array_test_data d;
                std::fill(std::begin(d.a1), std::end(d.a2), K);
                return d;
            }

            template <typename Iterator>
            static void init_a2(iterator_range<Iterator> range)
            {
                int i = 0;

                for (auto&& [a, b] : range)
                {
                    ASSERT_EQ(a, K);
                    b = a + i;
                    ++i;
                }
            }

            template <typename Iterator>
            static void check_result(iterator_range<Iterator> range)
            {
                int i = 0;

                for (auto&& [a, b] : range)
                {
                    ASSERT_EQ(a, K);
                    ASSERT_EQ(b, K + i);
                    ++i;
                }
            }
        };
    }

    TEST(zip_range, span_by_reference)
    {
        auto data = array_test_data::setup();

        std::span<const int> cs1{data.a1};
        std::span<int> s2{data.a2};
        std::span<const int> cs2{data.a2};

        array_test_data::init_a2(zip_range(cs1, s2));
        array_test_data::check_result(zip_range(cs1, cs2));
    }

    TEST(zip_range, c_array)
    {
        auto data = array_test_data::setup();

        array_test_data::init_a2(zip_range(data.a1, data.a2));
        array_test_data::check_result(zip_range(data.a1, data.a2));
    }

    TEST(zip_range, cpp_array)
    {
        auto data = array_test_data::setup();

        std::array<int, array_test_data::N> a1;
        std::array<int, array_test_data::N> a2;

        std::copy(std::begin(data.a1), std::end(data.a1), std::begin(a1));
        std::copy(std::begin(data.a2), std::end(data.a2), std::begin(a2));

        array_test_data::init_a2(zip_range(a1, a2));
        array_test_data::check_result(zip_range(a1, a2));
    }

    TEST(zip_range, vector)
    {
        auto data = array_test_data::setup();

        std::vector<int> a1{std::begin(data.a1), std::end(data.a1)};
        std::vector<int> a2{std::begin(data.a2), std::end(data.a2)};

        array_test_data::init_a2(zip_range(a1, a2));
        array_test_data::check_result(zip_range(a1, a2));
    }

    TEST(zip_range, span_by_value)
    {
        std::vector a = {1, 2, 3};
        std::vector b = {'a', 'b', 'c'};

        int n = 0;

        for (auto&& [i, c] : zip_range(std::span{a}, std::span{b}))
        {
            ASSERT_EQ(i, n + 1);
            ASSERT_EQ(c, char(n + 'a'));

            i = c + 'A' - 'a';
            ASSERT_EQ(i, n + 'A');

            ++n;
        }

        ASSERT_EQ(n, 3);
    }

    TEST(zip_iterator, sort)
    {
        constexpr auto N = 5;
        std::array<int, N> keys = {4, 0, 2, 1, 3};
        std::array<char, N> values = {'4', '0', '2', '1', '3'};

        auto begin = zip_iterator{keys.begin(), values.begin()};
        auto end = begin + N;

        std::sort(begin, end, [](auto&& lhs, auto&& rhs) { return std::get<0>(lhs) < std::get<0>(rhs); });

        constexpr std::array<int, N> expectedKeys = {0, 1, 2, 3, 4};
        constexpr std::array<char, N> expectedValues = {'0', '1', '2', '3', '4'};

        ASSERT_EQ(keys, expectedKeys);
        ASSERT_EQ(values, expectedValues);
    }

    TEST(reverse_iterator, dynamic_array)
    {
        constexpr auto N = 5;
        dynamic_array<u32> values{get_global_allocator(), {4, 3, 2, 1, 0}};

        u32 i = 0;

        const auto diff = (rend(values) - rbegin(values));
        ASSERT_EQ(diff, 5);

        for (auto it = rbegin(values); it != rend(values); ++it)
        {
            ASSERT_EQ(i, *it);
            ++i;
        }

        ASSERT_EQ(i, N);
    }

    TEST(reverse_range, dynamic_array)
    {
        constexpr auto N = 5;
        dynamic_array<u32> values{get_global_allocator(), {4, 3, 2, 1, 0}};

        u32 i = 0;

        for (u32 v : reverse_range(values))
        {
            ASSERT_EQ(i, v);
            ++i;
        }

        ASSERT_EQ(i, N);
    }

    enum class my_enum
    {
        a,
        b,
        c,
        d,
        e,
        enum_max,
    };

    TEST(flags_range, simple)
    {
        {
            const flags<my_enum> empty{};

            dynamic_array<my_enum> result;

            for (const my_enum e : flags_range{empty})
            {
                result.push_back(e);
            }

            ASSERT_TRUE(result.empty());
        }

        {
            const flags<my_enum> acd = my_enum::a | my_enum::c | my_enum::d;
            constexpr std::array expected = {my_enum::a, my_enum::c, my_enum::d};

            dynamic_array<my_enum> result;

            for (const my_enum e : flags_range{acd})
            {
                result.push_back(e);
            }

            ASSERT_EQ(result, expected);
        }

        {
            const flags<my_enum> all = my_enum::a | my_enum::b | my_enum::c | my_enum::d | my_enum::e;

            constexpr std::array expected = {
                my_enum::a,
                my_enum::b,
                my_enum::c,
                my_enum::d,
                my_enum::e,
            };

            dynamic_array<my_enum> result;

            for (const my_enum e : flags_range{all})
            {
                result.push_back(e);
            }

            ASSERT_EQ(result, expected);
        }
    }
}