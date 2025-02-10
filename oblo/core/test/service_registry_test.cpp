#include <gtest/gtest.h>

#include <oblo/core/service_registry.hpp>
#include <oblo/core/service_registry_builder.hpp>

namespace oblo
{
    namespace
    {
        struct service_foo
        {
            u32 value;
        };

        struct service_bar
        {
            service_foo foo;
        };
    }

    TEST(service_registry_builder, basic)
    {
        service_registry_builder builder;

        builder.add(+[](service_builder<service_foo> builder) { builder.unique(service_foo{42}); });

        service_registry registry;

        ASSERT_EQ(registry.find<service_foo>(), nullptr);

        ASSERT_TRUE(builder.build(registry));

        auto* foo = registry.find<service_foo>();
        ASSERT_NE(foo, nullptr);

        ASSERT_EQ(foo->value, 42);
    }

    TEST(service_registry_builder, dependency)
    {
        service_registry_builder builder;

        builder.add(
            +[](service_builder<service_bar> builder, const service_foo& foo) { builder.unique(service_bar{foo}); });

        builder.add(+[](service_builder<service_foo> builder) { builder.unique(service_foo{42}); });

        service_registry registry;

        ASSERT_EQ(registry.find<service_foo>(), nullptr);
        ASSERT_EQ(registry.find<service_bar>(), nullptr);

        ASSERT_TRUE(builder.build(registry));

        auto* foo = registry.find<service_foo>();
        ASSERT_NE(foo, nullptr);

        ASSERT_EQ(foo->value, 42);

        auto* bar = registry.find<service_bar>();
        ASSERT_NE(bar, nullptr);

        ASSERT_EQ(bar->foo.value, 42);
    }

    TEST(service_registry_builder, missing)
    {
        service_registry_builder builder;

        builder.add(
            +[](service_builder<service_bar> builder, const service_foo& foo) { builder.unique(service_bar{foo}); });

        service_registry registry;

        ASSERT_EQ(registry.find<service_foo>(), nullptr);
        ASSERT_EQ(registry.find<service_bar>(), nullptr);

        ASSERT_FALSE(builder.build(registry));

        ASSERT_EQ(registry.find<service_foo>(), nullptr);
        ASSERT_EQ(registry.find<service_bar>(), nullptr);
    }
}