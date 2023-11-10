#include <gtest/gtest.h>

#include <oblo/core/types.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/ecs/utility/registration.hpp>

#include <algorithm>
#include <bit>
#include <random>

#define ASSERT_ALIGNED(Span) ASSERT_EQ(std::bit_cast<std::uintptr_t>((Span).data()) % alignof(decltype((Span)[0])), 0)

namespace oblo::ecs
{
    namespace
    {
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

            instance_counted(const instance_counted& other) = delete;

            instance_counted(instance_counted&& other) noexcept
            {
                ++s_counter;
                value = other.value;
            }

            ~instance_counted()
            {
                --s_counter;
            }

            instance_counted& operator=(const instance_counted& other) = delete;

            instance_counted& operator=(instance_counted&& other) noexcept
            {
                value = other.value;
                return *this;
            }

            i64 value{-1};
        };

        using u32_c = u32;
        using string_c = std::string;
        using a_uvec4_c = aligned_uvec4;
    }

    class single_archetype_test : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const component_type stringComponent = register_type<string_c>(typeRegistry);
            const component_type instanceCountedComponent = register_type<instance_counted>(typeRegistry);
            const component_type u32Component = register_type<u32_c>(typeRegistry);
            const component_type alignedUvec4Component = register_type<a_uvec4_c>(typeRegistry);

            ASSERT_TRUE(stringComponent);
            ASSERT_TRUE(instanceCountedComponent);
            ASSERT_TRUE(u32Component);
            ASSERT_TRUE(alignedUvec4Component);

            for (auto iteration = 0; iteration < Iterations; ++iteration)
            {
                entity newEntities[N];
                reg.create<string_c, a_uvec4_c, instance_counted>(N, newEntities);

                ASSERT_TRUE(
                    std::all_of(std::begin(newEntities), std::end(newEntities), [](entity e) { return bool{e}; }));
                ASSERT_EQ(instance_counted::s_counter, N * (iteration + 1));

                for (int n = 0; n < N; ++n)
                {
                    const auto entityId = newEntities[n].value;
                    instance_counted& ic = reg.get<instance_counted>({entityId});
                    ASSERT_EQ(ic.value, i64(-1)) << "Iteration: " << iteration << " N: " << n;
                    ic.value = entityId;

                    a_uvec4_c& v = reg.get<a_uvec4_c>({entityId});

                    for (auto& u : v.data)
                    {
                        u = entityId % 2 ? 666 : 1337;
                    }

                    string_c& s = reg.get<string_c>({entityId});
                    s = std::to_string(entityId);
                }

                {
                    const std::span archetypes = reg.get_archetypes();
                    ASSERT_EQ(archetypes.size(), 1);
                }

                u32 totalEntities{0};

                // Iterate again to check that values are correct
                reg.range<a_uvec4_c, string_c, instance_counted>().for_each_chunk(
                    [&totalEntities](std::span<const entity> entities,
                        std::span<const a_uvec4_c> vectors,
                        std::span<const string_c> strings,
                        std::span<const instance_counted> ics)
                    {
                        ASSERT_ALIGNED(entities);
                        ASSERT_ALIGNED(vectors);
                        ASSERT_ALIGNED(strings);
                        ASSERT_ALIGNED(ics);

                        for (auto&& [e, v, s, ic] : zip_range(entities, vectors, strings, ics))
                        {
                            ASSERT_EQ(e.value, u32(ic.value));

                            for (auto& u : v.data)
                            {
                                ASSERT_EQ(u, e.value % 2 ? 666 : 1337) << "Entity: " << e.value;
                            }

                            ASSERT_EQ(s, std::to_string(e.value));
                            ++totalEntities;
                        }
                    });

                ASSERT_EQ(totalEntities, (iteration + 1) * N) << " Iteration: " << iteration;
            }
        }

        static constexpr auto Iterations = 64;
        static constexpr auto N = 33;

        type_registry typeRegistry;
        entity_registry reg{&typeRegistry};
    };

    TEST_F(single_archetype_test, clear)
    {
        ASSERT_EQ(instance_counted::s_counter, Iterations * N);
        reg = {};
        ASSERT_EQ(instance_counted::s_counter, 0);
    }

    TEST_F(single_archetype_test, destroy_half)
    {
        std::default_random_engine rng{42};

        std::vector<entity> entities;
        entities.reserve(Iterations * N);

        for (auto&& [chunkEntities, ics] : reg.range<instance_counted>())
        {
            entities.insert(entities.end(), chunkEntities.begin(), chunkEntities.end());

            for (auto&& [e, ic] : zip_range(chunkEntities, ics))
            {
                ASSERT_EQ(e.value, u32(ic.value));
            }
        }

        std::shuffle(entities.begin(), entities.end(), rng);

        const auto m = entities.begin() + entities.size() / 2;

        for (auto it = entities.begin(); it != m; ++it)
        {
            reg.destroy(*it);
        }

        ASSERT_EQ(instance_counted::s_counter, Iterations * N / 2);

        for (auto it = entities.begin(); it != m; ++it)
        {
            ASSERT_FALSE(reg.contains(*it));
        }

        for (auto it = m; it != entities.end(); ++it)
        {
            ASSERT_TRUE(reg.contains(*it));
            ASSERT_EQ(reg.get<instance_counted>(*it).value, it->value);
        }

        ASSERT_EQ(entities.size(), Iterations * N);
    }

    TEST_F(single_archetype_test, destroy_half_from_last)
    {
        std::vector<entity> entities;
        entities.reserve(Iterations * N);

        for (auto&& [chunkEntities, ics] : reg.range<instance_counted>())
        {
            entities.insert(entities.end(), chunkEntities.begin(), chunkEntities.end());

            for (auto&& [e, ic] : zip_range(chunkEntities, ics))
            {
                ASSERT_EQ(e.value, u32(ic.value));
            }
        }

        const auto m = entities.rbegin() + entities.size() / 2;

        for (auto it = entities.rbegin(); it != m; ++it)
        {
            reg.destroy(*it);
        }

        ASSERT_EQ(instance_counted::s_counter, Iterations * N / 2);

        for (auto it = entities.rbegin(); it != m; ++it)
        {
            ASSERT_FALSE(reg.contains(*it));
        }

        for (auto it = m; it != entities.rend(); ++it)
        {
            ASSERT_TRUE(reg.contains(*it));
            ASSERT_EQ(reg.get<instance_counted>(*it).value, it->value);
        }

        ASSERT_EQ(entities.size(), Iterations * N);
    }
}

#undef ASSERT_ALIGNED