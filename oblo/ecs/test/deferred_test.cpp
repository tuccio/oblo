#include <gtest/gtest.h>

#include <oblo/core/string/string.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
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

        struct tag_c
        {
        };

        struct alignas(16) aligned_uvec4
        {
            u32 data[4];

            bool operator==(const aligned_uvec4&) const = default;
        };

        struct component_with_reference
        {
            entity ref;
        };
    }

    TEST(deferred, basic)
    {
        type_registry types;
        entity_registry reg{&types};

        types.get_or_register_tag(make_tag_type_desc<tag_a>());
        types.get_or_register_tag(make_tag_type_desc<tag_b>());
        types.get_or_register_tag(make_tag_type_desc<tag_c>());
        types.get_or_register_component(make_component_type_desc<string>());
        types.get_or_register_component(make_component_type_desc<aligned_uvec4>());

        const auto e1 = reg.create();
        const auto e2 = reg.create();
        const auto e3 = reg.create();

        deferred d;

        {
            auto& stringRef = d.add<string>(e1);
            stringRef = "e1";
        }

        {
            auto&& [stringRef, uvec4Ref] = d.add<string, aligned_uvec4, tag_b>(e2);
            stringRef = "e2";
            uvec4Ref = aligned_uvec4{{1, 2, 3, 4}};
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

        {
            // Create new entity with unique tag_c (so we can recognize it)
            auto&& e4String = d.create<string, tag_c>();
            e4String = "e4";
        }

        d.apply(reg);

        {
            auto e4Range = reg.range<string>().with<tag_c>();
            ASSERT_EQ(e4Range.count(), 1);

            const std::span entities = (*e4Range.begin()).get<entity>();
            ASSERT_EQ(entities.size(), 1);

            const entity e4 = entities[0];
            ASSERT_TRUE(reg.contains(e4));
            ASSERT_TRUE(reg.has<string>(e4));

            ASSERT_EQ(reg.get<string>(e4), "e4");
        }
    }

    TEST(deferred, reserved_ids)
    {
        type_registry types;
        entity_registry reg{&types};

        types.get_or_register_component(make_component_type_desc<component_with_reference>());

        const entity e1 = reg.create();
        const entity e2 = reg.create();
        const entity e3 = reg.create();

        reg.add<component_with_reference>(e1).ref = e2;
        reg.add<component_with_reference>(e2).ref = e3;

        deferred d;

        auto&& [d1, c1] = d.create_with_reserved_id<component_with_reference>(reg);
        auto&& [d2, c2] = d.create_with_reserved_id<component_with_reference>(reg);

        reg.add<component_with_reference>(e3).ref = d1;

        c1.ref = d2;
        c2.ref = d1;

        ASSERT_TRUE(reg.contains(e1));
        ASSERT_TRUE(reg.contains(e2));
        ASSERT_TRUE(reg.contains(e3));

        ASSERT_FALSE(reg.contains(d1));
        ASSERT_FALSE(reg.contains(d2));

        d.apply(reg);

        ASSERT_TRUE(reg.contains(e1));
        ASSERT_TRUE(reg.contains(e2));
        ASSERT_TRUE(reg.contains(e3));

        ASSERT_TRUE(reg.contains(d1));
        ASSERT_TRUE(reg.contains(d2));

        ASSERT_TRUE(reg.has<component_with_reference>(e1));
        ASSERT_TRUE(reg.has<component_with_reference>(e2));
        ASSERT_TRUE(reg.has<component_with_reference>(e3));

        ASSERT_TRUE(reg.has<component_with_reference>(d1));
        ASSERT_TRUE(reg.has<component_with_reference>(d2));

        ASSERT_EQ(reg.get<component_with_reference>(e1).ref, e2);
        ASSERT_EQ(reg.get<component_with_reference>(e2).ref, e3);
        ASSERT_EQ(reg.get<component_with_reference>(e3).ref, d1);
        ASSERT_EQ(reg.get<component_with_reference>(d1).ref, d2);
        ASSERT_EQ(reg.get<component_with_reference>(d2).ref, d1);
    }
}