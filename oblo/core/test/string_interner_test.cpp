#include <gtest/gtest.h>

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_interner.hpp>

#include <random>
#include <unordered_map>

namespace oblo
{
    TEST(string_interner, basic)
    {
        string_interner interner;
        interner.init(32);

        ASSERT_FALSE(interner.get("A"_hsv));
        ASSERT_FALSE(interner.get("B"_hsv));
        ASSERT_FALSE(interner.get("C"_hsv));

        const auto a = interner.get_or_add("A"_hsv);
        const auto b = interner.get_or_add("B"_hsv);
        const auto c = interner.get_or_add("C"_hsv);

        ASSERT_TRUE(a);
        ASSERT_TRUE(b);
        ASSERT_TRUE(c);

        ASSERT_NE(a, b);
        ASSERT_NE(a, c);
        ASSERT_NE(b, c);

        ASSERT_EQ(a, interner.get("A"_hsv));
        ASSERT_EQ(b, interner.get("B"_hsv));
        ASSERT_EQ(c, interner.get("C"_hsv));

        ASSERT_EQ(interner.str(a), "A");
        ASSERT_EQ(interner.str(b), "B");
        ASSERT_EQ(interner.str(c), "C");

        ASSERT_EQ(interner.c_str(a), string_view{"A"});
        ASSERT_EQ(interner.c_str(b), string_view{"B"});
        ASSERT_EQ(interner.c_str(c), string_view{"C"});
    }

    TEST(string_interner, random)
    {
        string_interner interner;
        interner.init(32);

        const auto seed = std::random_device{}();

        std::cerr << "Seed: " << seed << "\n";

        std::default_random_engine rng{seed};
        std::uniform_int_distribution<u32> stringsCountDist{1u << 12, 1u << 16};
        std::uniform_int_distribution<u32> stringsLengthDist{0u, string_interner::MaxStringLength};
        std::uniform_int_distribution<u32> charDist{u32{'A'}, u32{'z'}};

        const auto stringsCount = stringsCountDist(rng);

        std::unordered_map<std::string, h32<string>> strings;
        strings.reserve(stringsCount);

        char buf[string_interner::MaxStringLength + 1];

        for (u32 i = 0; i < stringsCount; ++i)
        {
            const auto length = stringsLengthDist(rng);

            for (u32 j = 0; j < length; ++j)
            {
                buf[j] = char(charDist(rng));
            }

            buf[length] = '\0';
            const auto [it, inserted] = strings.emplace(buf, h32<string>{});

            if (inserted)
            {
                // Should be a new string
                ASSERT_FALSE(interner.get(hashed_string_view{buf})) << " Index: " << i << " String: " << buf;
                const auto handle = interner.get_or_add(hashed_string_view{buf});
                ASSERT_TRUE(handle);
                it->second = handle;
            }
            else
            {
                // The string should be already present
                const auto handle = interner.get(hashed_string_view{buf});
                ASSERT_TRUE(handle);
                ASSERT_EQ(it->second, handle);
            }
        }
    }
}