#include <gtest/gtest.h>

#include <oblo/core/flat_hash_map.hpp>
#include <oblo/core/hash.hpp>

#include <string>

namespace oblo
{
    // Hash that collapses many keys onto the same home slot to stress linear probing.
    struct mod8_hash
    {
        usize operator()(u32 k) const
        {
            return k % 8;
        }
    };

    TEST(flat_hash_map, insert_find_erase)
    {
        flat_hash_map<u32, u32> map;

        ASSERT_EQ(map.size(), 0u);
        ASSERT_TRUE(map.empty());

        for (u32 i = 0; i < 1000; ++i)
        {
            const auto [it, inserted] = map.emplace(i, i * 2);
            ASSERT_TRUE(inserted);
            ASSERT_EQ(it->second, i * 2);
        }

        ASSERT_EQ(map.size(), 1000u);
        ASSERT_FALSE(map.empty());

        for (u32 i = 0; i < 1000; ++i)
        {
            const auto it = map.find(i);
            ASSERT_NE(it, map.end());
            ASSERT_EQ(it->second, i * 2);
        }

        // Re-inserting the same key does not grow the map.
        for (u32 i = 0; i < 1000; ++i)
        {
            const auto [it, inserted] = map.emplace(i, i * 3);
            ASSERT_FALSE(inserted);
            ASSERT_EQ(it->second, i * 2);
        }

        ASSERT_EQ(map.size(), 1000u);

        for (u32 i = 0; i < 1000; ++i)
        {
            ASSERT_EQ(map.erase(i), 1u);
        }

        ASSERT_EQ(map.size(), 0u);
        ASSERT_TRUE(map.empty());

        for (u32 i = 0; i < 1000; ++i)
        {
            ASSERT_EQ(map.erase(i), 0u);
            ASSERT_EQ(map.find(i), map.end());
        }
    }

    TEST(flat_hash_map, collisions_and_growth)
    {
        // Many keys that collide on the low bits to exercise linear probing.
        flat_hash_map<u32, u32, mod8_hash> map;

        for (u32 i = 0; i < 4096; ++i)
        {
            map.emplace(i * 16, i);
        }

        for (u32 i = 0; i < 4096; ++i)
        {
            const auto it = map.find(i * 16);
            ASSERT_NE(it, map.end());
            ASSERT_EQ(it->second, i);
        }

        ASSERT_EQ(map.size(), 4096u);

        // Erase half, then re-insert to check tombstone handling and rehash.
        for (u32 i = 0; i < 4096; i += 2)
        {
            ASSERT_EQ(map.erase(i * 16), 1u);
        }

        for (u32 i = 0; i < 4096; i += 2)
        {
            map.emplace(i * 16, i * 7);
        }

        for (u32 i = 0; i < 4096; ++i)
        {
            const auto it = map.find(i * 16);
            ASSERT_NE(it, map.end());
            ASSERT_EQ(it->second, (i % 2 == 0) ? i * 7 : i);
        }
    }

    TEST(flat_hash_map, operator_subscript)
    {
        flat_hash_map<u32, std::string> map;

        map[1] = "one";
        map[2] = "two";

        ASSERT_EQ(map.size(), 2u);
        ASSERT_EQ(map[1], "one");
        ASSERT_EQ(map[2], "two");

        // operator[] on a missing key inserts a default value.
        ASSERT_EQ(map[3], "");
        ASSERT_EQ(map.size(), 3u);
    }

    TEST(flat_hash_map, non_trivial_key_value)
    {
        flat_hash_map<std::string, u32> map;

        map.emplace("apple", 1);
        map.emplace("banana", 2);
        map.emplace("cherry", 3);

        ASSERT_EQ(map.find("apple")->second, 1u);
        ASSERT_EQ(map.find("banana")->second, 2u);
        ASSERT_EQ(map.find("cherry")->second, 3u);
        ASSERT_EQ(map.find("durian"), map.end());

        map.erase("banana");
        ASSERT_EQ(map.find("banana"), map.end());
        ASSERT_NE(map.find("apple"), map.end());
    }

    TEST(flat_hash_map, iteration)
    {
        flat_hash_map<u32, u32> map;

        for (u32 i = 0; i < 500; ++i)
        {
            map.emplace(i, i);
        }

        u32 visited = 0;
        u32 sum = 0;

        for (const auto& e : map)
        {
            sum += e.second;
            ++visited;
        }

        ASSERT_EQ(visited, 500u);
        ASSERT_EQ(sum, 500u * 499u / 2u);
    }

    TEST(flat_hash_map, clear)
    {
        flat_hash_map<u32, u32> map;

        for (u32 i = 0; i < 100; ++i)
        {
            map.emplace(i, i);
        }

        map.clear();
        ASSERT_EQ(map.size(), 0u);
        ASSERT_TRUE(map.empty());

        for (u32 i = 0; i < 100; ++i)
        {
            ASSERT_EQ(map.find(i), map.end());
        }
    }

    TEST(flat_hash_map, copy_and_move)
    {
        flat_hash_map<u32, u32, mod8_hash> base;

        for (u32 i = 0; i < 200; ++i)
        {
            base.emplace(i, i * 2);
        }

        flat_hash_map<u32, u32, mod8_hash> copy = base;
        ASSERT_EQ(copy.size(), 200u);

        for (u32 i = 0; i < 200; ++i)
        {
            ASSERT_EQ(copy.find(i)->second, i * 2);
        }

        copy.emplace(999, 1);
        ASSERT_EQ(base.size(), 200u);

        flat_hash_map<u32, u32, mod8_hash> moved = std::move(base);
        ASSERT_EQ(moved.size(), 200u);
        ASSERT_EQ(base.size(), 0u);

        for (u32 i = 0; i < 200; ++i)
        {
            ASSERT_EQ(moved.find(i)->second, i * 2);
        }
    }
}
