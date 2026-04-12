#include <gtest/gtest.h>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/systems/system_descriptor.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/ecs/systems/system_seq_executor.hpp>
#include <oblo/ecs/systems/system_trampoline.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>

namespace oblo::ecs
{
    namespace
    {
        i64 s_systemOutputSink = 0;

        struct system_a
        {
            void update(const system_update_context&)
            {
                s_systemOutputSink = 'A';
            }
        };

        struct system_b
        {
            void update(const system_update_context&)
            {
                s_systemOutputSink = 'B';
            }
        };
    }

    TEST(system_trampoline_test, reload)
    {
        type_registry types;
        entity_registry entities{&types};

        intrusive_list<system_trampoline> instantiatedSystems{};

        system_graph_builder builder{system_graph_usages{}};
        builder.add_system_trampoline<system_a>(&instantiatedSystems);

        ASSERT_TRUE(instantiatedSystems.empty());
        const expected g = builder.build();

        ASSERT_TRUE(g);

        expected executor = g->instantiate();
        ASSERT_TRUE(executor);

        ASSERT_FALSE(instantiatedSystems.empty());

        ASSERT_EQ(s_systemOutputSink, 0);

        const system_update_context ctx{
            .entities = &entities,
        };

        executor->update(ctx);
        ASSERT_EQ(s_systemOutputSink, 'A');
        s_systemOutputSink = 0;

        executor->update(ctx);
        ASSERT_EQ(s_systemOutputSink, 'A');
        s_systemOutputSink = 0;

        swap_system_trampolines(instantiatedSystems, make_system_descriptor<system_b>());

        executor->update(ctx);
        ASSERT_EQ(s_systemOutputSink, 'B');
    }
}