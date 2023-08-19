#include <gtest/gtest.h>

#include <oblo/core/types.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
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

        struct alignas(16) aligned_uvec4
        {
            u32 data[4];
        };

        class instance_counted
        {
        public:
            inline static u32 s_counter{0};

            instance_counted()
            {
                ++s_counter;
            }

            instance_counted(const instance_counted&)
            {
                ++s_counter;
            }

            ~instance_counted()
            {
                --s_counter;
            }

            instance_counted& operator=(const instance_counted&) = delete;

            i64 value{-1};
        };

        using u32_c = u32;
        using string_c = std::string;
        using a_uvec4_c = aligned_uvec4;
    }

    TEST(entity_registry, basic)
    {
        type_registry typeRegistry;

        const component_type u32Component = register_type<u32_c>(typeRegistry);
        const component_type stringComponent = register_type<string_c>(typeRegistry);
        const component_type alignedUvec4Component = register_type<a_uvec4_c>(typeRegistry);
        const component_type instanceCountedComponent = register_type<instance_counted>(typeRegistry);

        ASSERT_TRUE(u32Component);
        ASSERT_TRUE(stringComponent);
        ASSERT_TRUE(alignedUvec4Component);
        ASSERT_TRUE(instanceCountedComponent);

        entity_registry reg{&typeRegistry};

        constexpr auto Iterations = 64;
        constexpr auto N = 33;

        for (auto iteration = 0; iteration < Iterations; ++iteration)
        {
            const auto newEntity = reg.create<string_c, a_uvec4_c, instance_counted>(N);

            ASSERT_TRUE(newEntity);
            ASSERT_EQ(instance_counted::s_counter, N * (iteration + 1));

            for (int n = 0; n < N; ++n)
            {
                instance_counted& ic = reg.get<instance_counted>({newEntity.value + n});
                ASSERT_EQ(ic.value, i64(-1)) << "Iteration: " << iteration << " N: " << n;
                ic.value = n;
            }

            reg.range<a_uvec4_c, string_c>().for_each_chunk(
                [newEntity](std::span<const entity> entities,
                            std::span<a_uvec4_c> vectors,
                            std::span<string_c> strings,
                            std::span<const instance_counted> countedInstances)
                {
                    for (auto&& [e, v, s, ic] : zip_range(entities, vectors, strings, countedInstances))
                    {
                        const auto currentIndex = u32(&e - entities.data());
                        ASSERT_EQ(e.value, newEntity.value + currentIndex);
                        ASSERT_EQ(ic.value, i64(currentIndex));

                        for (auto& u : v.data)
                        {
                            u = e.value;
                        }

                        s = std::to_string(e.value);
                    }
                });

            reg.range<a_uvec4_c, string_c>().for_each_chunk(
                [newEntity](std::span<const entity> entities,
                            std::span<const a_uvec4_c> vectors,
                            std::span<const string_c> strings)
                {
                    for (auto&& [e, v, s] : zip_range(entities, vectors, strings))
                    {
                        const auto currentIndex = u32(&e - entities.data());
                        ASSERT_EQ(e.value, newEntity.value + currentIndex);

                        for (auto& u : v.data)
                        {
                            ASSERT_EQ(u, e.value);
                        }

                        ASSERT_EQ(s, std::to_string(e.value));
                    }
                });
        }

        reg = {};

        ASSERT_EQ(instance_counted::s_counter, 0);
    }
}
