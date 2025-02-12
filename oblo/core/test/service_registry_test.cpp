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

        struct service_baz
        {
            service_foo foo;
            service_bar bar;
        };
    }

    TEST(service_registry_builder, basic)
    {
        service_registry_builder builder;

        builder.add<service_foo>().build([](service_builder<service_foo> builder) { builder.unique(service_foo{42}); });

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

        builder.add<service_bar>().require<const service_foo>().build(
            [](service_builder<service_bar> builder)
            {
                const service_foo& foo = *builder.find<const service_foo>();
                builder.unique(service_bar{foo});
            });

        builder.add<service_foo>().build([](service_builder<service_foo> builder) { builder.unique(service_foo{42}); });

        builder.add<service_baz>().require<const service_foo, service_bar>().build(
            [](service_builder<service_baz> builder)
            {
                const service_foo& foo = *builder.find<const service_foo>();
                const service_bar& bar = *builder.find<const service_bar>();

                builder.unique(service_baz{foo, bar});
            });

        service_registry registry;

        ASSERT_EQ(registry.find<service_foo>(), nullptr);
        ASSERT_EQ(registry.find<service_bar>(), nullptr);
        ASSERT_EQ(registry.find<service_baz>(), nullptr);

        ASSERT_TRUE(builder.build(registry));

        auto* foo = registry.find<service_foo>();
        ASSERT_NE(foo, nullptr);

        ASSERT_EQ(foo->value, 42);

        auto* bar = registry.find<service_bar>();
        ASSERT_NE(bar, nullptr);

        ASSERT_EQ(bar->foo.value, 42);

        auto* baz = registry.find<service_baz>();
        ASSERT_NE(baz, nullptr);

        ASSERT_EQ(baz->foo.value, 42);
        ASSERT_EQ(baz->bar.foo.value, 42);
    }

    TEST(service_registry_builder, missing)
    {
        service_registry_builder builder;

        builder.add<service_bar>().require<const service_foo>().build(
            [](service_builder<service_bar> builder)
            {
                const service_foo& foo = *builder.find<const service_foo>();
                builder.unique(service_bar{foo});
            });

        service_registry registry;

        ASSERT_EQ(registry.find<service_foo>(), nullptr);
        ASSERT_EQ(registry.find<service_bar>(), nullptr);

        const auto e = builder.build(registry);
        ASSERT_FALSE(e);
        ASSERT_EQ(e.error(), service_build_error::missing_dependency);

        ASSERT_EQ(registry.find<service_foo>(), nullptr);
        ASSERT_EQ(registry.find<service_bar>(), nullptr);
    }
}