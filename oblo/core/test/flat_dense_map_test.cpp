#include <gtest/gtest.h>

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle_pool.hpp>

namespace oblo
{
    TEST(flat_dense_map, flat_dense_map_emplace_erase)
    {
        constexpr auto max = 4096;
        bool alreadyInserted[max]{false};

        flat_dense_map<u32, std::string> map;

        for (u32 increments : {5, 3, 2, 1})
        {
            for (u32 index = 0; index < max; index += increments)
            {
                auto value = std::to_string(index);
                const auto [it, inserted] = map.emplace(index, value);

                ASSERT_NE(inserted, alreadyInserted[index]);
                alreadyInserted[index] = true;

                ASSERT_EQ(*it, value);
            }
        }

        for (u32 increments : {5, 3, 2, 1})
        {
            for (u32 index = 0; index < max; index += increments)
            {
                const bool erased = map.erase(index);

                ASSERT_EQ(erased, alreadyInserted[index]) << index << " " << increments;
                alreadyInserted[index] = false;
            }
        }
    }

    TEST(flat_dense_set, flat_dense_set_emplace_erase)
    {
        constexpr auto max = 4096;
        bool alreadyInserted[max]{false};

        flat_dense_set<u32> set;

        for (u32 increments : {5, 3, 2, 1})
        {
            for (u32 index = 0; index < max; index += increments)
            {
                auto value = index;
                const auto [it, inserted] = set.emplace(index);

                ASSERT_NE(inserted, alreadyInserted[index]);
                alreadyInserted[index] = true;

                ASSERT_EQ(*it, value);
            }
        }

        for (u32 increments : {5, 3, 2, 1})
        {
            for (u32 index = 0; index < max; index += increments)
            {
                const bool erased = set.erase(index);

                ASSERT_EQ(erased, alreadyInserted[index]) << index << " " << increments;
                alreadyInserted[index] = false;
            }
        }
    }

    TEST(handle_pool, handle_pool_generation)
    {
        handle_pool<u32, 2> pool;

        const auto a1 = pool.acquire();
        const auto b = pool.acquire();

        ASSERT_NE(a1, b);

        pool.release(a1);
        const auto a2 = pool.acquire();

        ASSERT_NE(a2, a1);
        ASSERT_NE(a2, b);

        pool.release(a2);
        const auto a3 = pool.acquire();

        ASSERT_NE(a3, a1);
        ASSERT_NE(a3, a2);
        ASSERT_NE(a3, b);

        pool.release(a3);
        const auto a4 = pool.acquire();

        ASSERT_NE(a4, a1);
        ASSERT_NE(a4, a2);
        ASSERT_NE(a4, a3);
        ASSERT_NE(a4, b);

        pool.release(a4);
        const auto a5 = pool.acquire();

        ASSERT_NE(a4, a2);
        ASSERT_NE(a4, a3);
        ASSERT_NE(a5, a4);
        ASSERT_NE(a4, b);
    }
}