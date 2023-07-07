#include <gtest/gtest.h>

#include <oblo/core/flat_dense_map.hpp>

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
}