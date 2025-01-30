#include <gtest/gtest.h>

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

        const auto e1 = reg.create<string>();
        const auto e2 = reg.create<string, u32>();
        const auto e3 = reg.create<u32>();

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
}