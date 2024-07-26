#include <gtest/gtest.h>

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo
{
    TEST(string_view, default_constructor)
    {
        string_view sv;
        EXPECT_EQ(sv.data(), nullptr);
        EXPECT_EQ(sv.size(), 0);
        EXPECT_TRUE(sv.empty());
    }

    TEST(string_view, from_c_string)
    {
        constexpr const char* cstr = "hello";
        string_view sv{cstr};
        EXPECT_EQ(sv.size(), 5);
        EXPECT_EQ(sv, "hello");
    }

    TEST(string_view, from_u8_c_string)
    {
        constexpr const char8_t* cstr = u8"hello";
        string_view sv{cstr};
        EXPECT_EQ(sv.size(), 5);
        EXPECT_EQ(sv, "hello");
    }

    TEST(string_view, substring)
    {
        string_view sv{"hello world"};
        string_view substr = sv.substr(0, 5);
        EXPECT_EQ(substr, "hello");

        substr = sv.substr(6, 5);
        EXPECT_EQ(substr, "world");
    }

    TEST(string_view, find)
    {
        string_view sv{"hello world"};
        EXPECT_EQ(sv.find("world"), 6);
        EXPECT_EQ(sv.find('o'), 4);
        EXPECT_EQ(sv.find("not there"), string_view::npos);
    }

    TEST(string_view, compare)
    {
        string_view sv1{"apple"};
        string_view sv2{"apple"};
        string_view sv3{"banana"};

        EXPECT_EQ(sv1.compare(sv2), 0);
        EXPECT_LT(sv1.compare(sv3), 0);
        EXPECT_GT(sv3.compare(sv1), 0);
    }

    TEST(string_view, remove_prefix_suffix)
    {
        string_view sv{"  trim me  "};
        sv.remove_prefix(2); // removes leading spaces
        sv.remove_suffix(2); // removes trailing spaces
        EXPECT_EQ(sv, "trim me");
    }

    TEST(string_view, front_back)
    {
        string_view sv{"test"};
        EXPECT_EQ(sv.front(), 't');
        EXPECT_EQ(sv.back(), 't');
    }

    TEST(cstring_view_test, default_constructor)
    {
        cstring_view sv{};
        EXPECT_NE(sv.data(), nullptr);
        EXPECT_EQ(sv.size(), 0);
        EXPECT_TRUE(sv.empty());
        EXPECT_EQ(std::strcmp(sv.c_str(), ""), 0);
    }

    TEST(cstring_view_test, from_c_string)
    {
        const char* cstr = "hello";
        cstring_view sv{cstr};
        EXPECT_EQ(sv.size(), 5);
        EXPECT_EQ(sv, "hello");
    }

    TEST(cstring_view_test, from_c_string_with_null_char)
    {
        const char* cstr = "hello\0world";
        cstring_view sv{cstr};
        EXPECT_EQ(sv.size(), 5); // Only counts until the first null character
        EXPECT_EQ(sv, "hello");
    }

    TEST(cstring_view_test, comparison)
    {
        cstring_view sv1{"apple"};
        cstring_view sv2{"apple"};
        cstring_view sv3{"banana"};

        EXPECT_EQ(sv1.compare(sv2), 0);
        EXPECT_LT(sv1.compare(sv3), 0);
        EXPECT_GT(sv3.compare(sv1), 0);
    }

    TEST(cstring_view_test, find)
    {
        cstring_view sv{"hello world"};
        EXPECT_EQ(sv.find("world"), 6);
        EXPECT_EQ(sv.find('o'), 4);
        EXPECT_EQ(sv.find("not there"), cstring_view::npos);
    }

    TEST(cstring_view_test, front_back)
    {
        cstring_view sv{"test"};
        EXPECT_EQ(sv.front(), 't');
        EXPECT_EQ(sv.back(), 't');
    }

    TEST(cstring_view_test, modifying_original_string)
    {
        char str[] = "original";
        cstring_view sv{str};
        str[0] = 'O';              // Modify the original string
        EXPECT_EQ(sv, "Original"); // cstring_view reflects this change
    }

    TEST(cstring_view_test, c_str)
    {
        const char* cstr = "cstring_view test";
        cstring_view sv{cstr};
        EXPECT_EQ(sv.c_str(), cstr);            // c_str() should return the original pointer
        EXPECT_EQ(strcmp(sv.c_str(), cstr), 0); // Compare using C-style string comparison
    }

    TEST(hashed_string_view_test, default_constructor)
    {
        hashed_string_view hsv{};
        EXPECT_EQ(hsv.data(), nullptr);
        EXPECT_EQ(hsv.size(), 0);
        EXPECT_TRUE(hsv.empty());
        EXPECT_EQ(hsv.hash(), hash_xxhz(hsv.data(), hsv.size()));
    }

    TEST(hashed_string_view_test, from_string_view)
    {
        string_view sv = "hello";
        hashed_string_view hsv{sv};

        EXPECT_EQ(hsv, sv);
        EXPECT_EQ(hsv.hash(), "hello"_hsv.hash());
    }

    TEST(hashed_string_view_test, from_c_string)
    {
        const char* cstr = "world";
        string_view sv{cstr};
        hashed_string_view hsv{sv};

        EXPECT_EQ(hsv, sv);
        EXPECT_EQ(hsv.hash(), "world"_hsv.hash());
    }

    TEST(hashed_string_view_test, comparison)
    {
        hashed_string_view hsv1{"apple"};
        hashed_string_view hsv2{"apple"};
        hashed_string_view hsv3{"banana"};

        EXPECT_EQ(hsv1, hsv2);
        EXPECT_NE(hsv1, hsv3);
        EXPECT_EQ(hsv1.hash(), hsv2.hash());
        EXPECT_NE(hsv1.hash(), hsv3.hash());
    }

    TEST(hashed_string_view_test, assignment_operator)
    {
        hashed_string_view hsv1{"initial"};
        hashed_string_view hsv2{"modified"};

        hsv1 = hsv2; // Assignment operation

        EXPECT_EQ(hsv1, hsv2);
        EXPECT_EQ(hsv1.hash(), hsv2.hash());
    }

    TEST(hashed_string_view_test, empty_string)
    {
        hashed_string_view hsv{};

        EXPECT_EQ(hsv.hash(), ""_hsv.hash());
        EXPECT_EQ(hsv, ""_hsv);

        string_view sv;
        EXPECT_EQ(sv, ""_hsv);
    }

    TEST(hashed_string_view_test, hash_consistency)
    {
        const auto hsvh1 = hash_all<hash>("hello world"_hsv);
        const auto svh1 = hash_all<hash>("hello world"_sv);
        const auto csvh1 = hash_all<hash>("hello world"_csv);

        EXPECT_EQ(hsvh1, svh1);
        EXPECT_EQ(hsvh1, csvh1);

        const auto hsvh2 = hash_all<hash>(""_hsv);
        const auto svh2 = hash_all<hash>(""_sv);
        const auto csvh2 = hash_all<hash>(""_csv);

        EXPECT_EQ(hsvh2, svh2);
        EXPECT_EQ(hsvh2, csvh2);
    }
}