#include <gtest/gtest.h>

#include <oblo/core/types.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
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

        struct u8_component
        {
            u8 foo;
        };

        struct i32_component
        {
            i32 foo;
        };

        struct f64_component
        {
            f64 foo;
        };
    }

    TEST(type_registry, registration)
    {
        type_registry typeRegistry;

        const auto u8ComponentHandle = typeRegistry.register_component(make_component_type_desc<u8_component>());
        const auto i32ComponentHandle = typeRegistry.register_component(make_component_type_desc<i32_component>());
        const auto f64ComponentHandle = typeRegistry.register_component(make_component_type_desc<f64_component>());

        ASSERT_TRUE(u8ComponentHandle);
        ASSERT_TRUE(i32ComponentHandle);
        ASSERT_TRUE(f64ComponentHandle);

        ASSERT_FALSE(typeRegistry.register_component(make_component_type_desc<u8_component>()));
        ASSERT_FALSE(typeRegistry.register_component(make_component_type_desc<i32_component>()));
        ASSERT_FALSE(typeRegistry.register_component(make_component_type_desc<f64_component>()));

        const auto tagAHandle = typeRegistry.register_tag(make_tag_type_desc<tag_a>());
        const auto tagBHandle = typeRegistry.register_tag(make_tag_type_desc<tag_b>());
        const auto tagCHandle = typeRegistry.register_tag(make_tag_type_desc<tag_c>());

        ASSERT_TRUE(tagAHandle);
        ASSERT_TRUE(tagBHandle);
        ASSERT_TRUE(tagCHandle);

        ASSERT_FALSE(typeRegistry.register_tag(make_tag_type_desc<tag_a>()));
        ASSERT_FALSE(typeRegistry.register_tag(make_tag_type_desc<tag_b>()));
        ASSERT_FALSE(typeRegistry.register_tag(make_tag_type_desc<tag_c>()));
    }

    TEST(type_registry, type_set)
    {
        type_registry typeRegistry;

        const auto u8ComponentHandle = typeRegistry.register_component(make_component_type_desc<u8_component>());
        const auto i32ComponentHandle = typeRegistry.register_component(make_component_type_desc<i32_component>());
        const auto f64ComponentHandle = typeRegistry.register_component(make_component_type_desc<f64_component>());

        ASSERT_TRUE(u8ComponentHandle);
        ASSERT_TRUE(i32ComponentHandle);
        ASSERT_TRUE(f64ComponentHandle);

        const auto tagAHandle = typeRegistry.register_tag(make_tag_type_desc<tag_a>());
        const auto tagBHandle = typeRegistry.register_tag(make_tag_type_desc<tag_b>());
        const auto tagCHandle = typeRegistry.register_tag(make_tag_type_desc<tag_c>());

        ASSERT_TRUE(tagAHandle);
        ASSERT_TRUE(tagBHandle);
        ASSERT_TRUE(tagCHandle);

        const auto [components, tags] = make_type_sets<u8_component, tag_b, f64_component>(typeRegistry);

        ASSERT_TRUE(components.contains(u8ComponentHandle));
        ASSERT_FALSE(components.contains(i32ComponentHandle));
        ASSERT_TRUE(components.contains(f64ComponentHandle));

        ASSERT_FALSE(tags.contains(tagAHandle));
        ASSERT_TRUE(tags.contains(tagBHandle));
        ASSERT_FALSE(tags.contains(tagCHandle));
    }
}
