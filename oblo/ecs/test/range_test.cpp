#include <gtest/gtest.h>

#include <oblo/core/random_generator.hpp>
#include <oblo/core/reflection/struct_compare.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/utility/registration.hpp>

namespace oblo::ecs
{
    TEST(range, notify_basic)
    {
        type_registry types;
        entity_registry reg{&types};

        register_type<string>(types);
        register_type<u32>(types);

        [[maybe_unused]] const auto e1 = reg.create<string>();
        [[maybe_unused]] const auto e2 = reg.create<string, u32>();
        [[maybe_unused]] const auto e3 = reg.create<u32>();

        reg.set_modification_id(1u);

        ASSERT_EQ(reg.range<u32>().notified().count(), 0u);

        for (auto&& chunk : reg.range<u32>())
        {
            for (auto& v : chunk.get<u32>())
            {
                v = 42;
            }

            chunk.notify(true);
        }

        ASSERT_EQ(reg.range<u32>().notified().count(), 2u);

        for (auto&& chunk : reg.range<u32>().notified())
        {
            for (auto& v : chunk.get<u32>())
            {
                ASSERT_EQ(v, 42);
            }
        }

        ASSERT_EQ(reg.range<u32>().notified().count(), 2u);

        reg.set_modification_id(2u);

        ASSERT_EQ(reg.range<u32>().notified().count(), 0u);
    }

    TEST(range, notify_randomized)
    {
        type_registry types;
        entity_registry reg{&types};

        register_type<f32>(types);
        register_type<u32>(types);

        constexpr u32 numEntitiesPerArchetype = 1u << 14;

        deque<entity> u32Entities;

        reg.set_modification_id(0u);

        for (u32 i = 0; i < numEntitiesPerArchetype; ++i)
        {
            reg.create<f32>();
        }

        for (u32 i = 0; i < numEntitiesPerArchetype; ++i)
        {
            const auto e = reg.create<u32>();
            reg.get<u32>(e) = 0;
            u32Entities.emplace_back(e);
        }

        for (u32 i = 0; i < numEntitiesPerArchetype; ++i)
        {
            const auto e = reg.create<u32, f32>();
            reg.get<u32>(e) = 0;
            u32Entities.emplace_back(e);
        }

        ASSERT_EQ(reg.range<u32>().notified().count(), 2 * numEntitiesPerArchetype);

        reg.set_modification_id(1u);

        ASSERT_EQ(reg.range<u32>().notified().count(), 0);

        random_generator rng;
        rng.seed(42);

        for (u32 round = 0; round < 100; ++round)
        {
            const auto maxChanges = uniform_distribution<u32>{0, 1000}(rng);

            const u32 roundValue = 42 + round;

            u32 totalChanges = 0;

            for (u32 change = 0; change < maxChanges; ++change)
            {
                const auto entityIndex = uniform_distribution<usize>{0u, u32Entities.size() - 1}.generate(rng);

                const auto e = u32Entities[entityIndex];

                auto& v = reg.get<u32>(e);

                if (v != roundValue)
                {
                    v = roundValue;
                    ++totalChanges;
                    reg.notify(e);
                }
            }

            // Check that all the entities we modified are notified, and that we are not iterating everything

            {
                u32 iteratedChunks = 0;
                u32 foundChanges = 0;

                for (auto&& chunk : reg.range<u32>().notified())
                {
                    ++iteratedChunks;

                    u32 changesInChunk = 0;

                    for (auto& v : chunk.get<u32>())
                    {
                        if (v == roundValue)
                        {
                            ++changesInChunk;
                            ++foundChanges;
                        }
                    }

                    ASSERT_GT(changesInChunk, 0);
                }

                ASSERT_EQ(foundChanges, totalChanges);
                ASSERT_LE(iteratedChunks, totalChanges);
            }

            // Same test but using for_each_chunk

            {
                u32 iteratedChunks = 0;
                u32 foundChanges = 0;

                reg.range<u32>().notified().for_each_chunk(
                    [&](std::span<const entity>, std::span<const u32> values)
                    {
                        ++iteratedChunks;

                        u32 changesInChunk = 0;

                        for (auto& v : values)
                        {
                            if (v == roundValue)
                            {
                                ++changesInChunk;
                                ++foundChanges;
                            }
                        }

                        ASSERT_GT(changesInChunk, 0);
                    });

                ASSERT_EQ(foundChanges, totalChanges);
                ASSERT_LE(iteratedChunks, totalChanges);
            }

            reg.set_modification_id(reg.get_modification_id() + 1);
            ASSERT_EQ(reg.range<u32>().notified().count(), 0);
        }
    }
}