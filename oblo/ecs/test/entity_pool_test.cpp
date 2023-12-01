#include <gtest/gtest.h>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>

namespace oblo::ecs
{
    TEST(entity_pool_test, small_number)
    {
        type_registry typeRegistry;
        entity_registry reg{&typeRegistry};

        constexpr u32 N{63};
        // +1 because id 0 is invalid, then -1 make it a mask (since it's a power of two)
        constexpr u32 Mask{((N + 1) << 1) - 1};

        entity entities[N]{};

        for (u32 i = 0; i < N; ++i)
        {
            const auto e = reg.create<>();

            ASSERT_TRUE(e);
            ASSERT_TRUE(reg.contains(e));

            const auto index = e.value & Mask;
            ASSERT_LE(index, N);

            entities[i] = e;
        }

        for (u32 i = 0; i < N; i += 2)
        {
            const auto e = entities[i];
            reg.destroy(e);

            ASSERT_FALSE(reg.contains(e));

            entities[i] = {};
        }

        for (u32 i = 0; i < N; i += 2)
        {
            const auto e = reg.create<>();

            ASSERT_TRUE(e);
            ASSERT_TRUE(reg.contains(e));

            const auto index = e.value & Mask;
            ASSERT_LE(index, N);

            entities[i] = e;
        }
    }
}