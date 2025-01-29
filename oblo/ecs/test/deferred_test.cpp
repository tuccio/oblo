#include <gtest/gtest.h>

#include <oblo/core/string/string.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/ecs/utility/registration.hpp>

namespace oblo::ecs
{
    namespace
    {
        struct tag_a
        {
        };

        struct tag_b
        {
        };

        struct alignas(16) aligned_uvec4
        {
            u32 data[4];

            bool operator==(const aligned_uvec4&) const = default;
        };
    }

    TEST(deferred, basic)
    {
        type_registry types;
        entity_registry reg{&types};

        types.get_or_register_tag(make_tag_type_desc<tag_a>());
        types.get_or_register_tag(make_tag_type_desc<tag_b>());
        types.get_or_register_component(make_component_type_desc<string>());
        types.get_or_register_component(make_component_type_desc<aligned_uvec4>());

        const auto e1 = reg.create();
        const auto e2 = reg.create();
        const auto e3 = reg.create();

        deferred d;

        {
            const auto [stringPtr] = d.add<string>(e1);

            ASSERT_TRUE(stringPtr);
            *stringPtr = "e1";
        }

        {
            const auto [stringPtr, uvec4Ptr] = d.add<string, aligned_uvec4, tag_b>(e2);
            ASSERT_TRUE(uvec4Ptr);
            *stringPtr = "e2";
            *uvec4Ptr = aligned_uvec4{{1, 2, 3, 4}};
        }

        {
            d.add<tag_a, tag_b>(e3);
        }

        ASSERT_FALSE(reg.has<string>(e1));
        ASSERT_FALSE((reg.has<string, aligned_uvec4, tag_b>(e2)));
        ASSERT_FALSE((reg.has<tag_a, tag_b>(e3)));

        d.apply(reg);

        ASSERT_TRUE(reg.has<string>(e1));
        ASSERT_TRUE((reg.has<string, aligned_uvec4, tag_b>(e2)));
        ASSERT_TRUE((reg.has<tag_a, tag_b>(e3)));

        ASSERT_EQ(reg.get<string>(e1), "e1");
        ASSERT_EQ(reg.get<string>(e2), "e2");
        ASSERT_EQ(reg.get<aligned_uvec4>(e2), (aligned_uvec4{{1, 2, 3, 4}}));

        // Remove string from e2
        d.remove<string>(e2);
        d.apply(reg);

        ASSERT_FALSE(reg.has<string>(e2));
        ASSERT_TRUE(reg.has<aligned_uvec4>(e2));

        // Destroy e1
        d.destroy(e1);
        d.apply(reg);

        ASSERT_FALSE(reg.contains(e1));
    }
}