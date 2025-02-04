#include <gtest/gtest.h>

#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/iterator/enum_range.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/registrant.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/name_component.hpp>
#include <oblo/scene/components/parent_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>
#include <oblo/scene/components/tags.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
}

namespace oblo
{
    namespace
    {
        class ecs_serialization_test : public testing::Test
        {
        protected:
            static void SetUpTestSuite()
            {
                filesystem::remove_all(testDir).assert_value();
                filesystem::create_directories(testDir).assert_value();
            }

            ecs_serialization_test() : reflectionRegistry{mm.load<reflection::reflection_module>()->get_registry()}
            {
                mm.load<scene_module>();

                propertyRegistry.init(reflectionRegistry);
            }

            void register_from_reflection()
            {
                ecs_utility::register_reflected_component_and_tag_types(reflectionRegistry,
                    &typeRegistry,
                    &propertyRegistry);
            }

            reflection::reflection_registry::registrant get_reflection_registrant()
            {
                auto* const reflection = mm.load<reflection::reflection_module>();
                return reflection->get_registrant();
            }

            static constexpr cstring_view testDir{"./test/ecs_serializer/"};

            module_manager mm;
            const reflection::reflection_registry& reflectionRegistry;
            ecs::type_registry typeRegistry;
            property_registry propertyRegistry;
        };
    }

    TEST_F(ecs_serialization_test, json_serialization)
    {
        register_from_reflection();

        enum class test_mode
        {
            no_flags,
            skip_transitive,
            skip_global_transform,
            enum_max,
        };

        for (auto mode : enum_range<test_mode>())
        {
            ecs_serializer::write_config cfg;
            string_builder jsonPath;

            switch (mode)
            {
            case test_mode::no_flags:
                jsonPath.append(testDir).append_path("json_serialization_no_flags.json");
                break;

            case test_mode::skip_transitive:
                cfg.skipEntities.tags.add(typeRegistry.find_tag<transient_tag>());
                jsonPath.append(testDir).append_path("json_serialization_skip_transitive.json");
                break;

            case test_mode::skip_global_transform:
                cfg.skipTypes.components.add(typeRegistry.find_component<global_transform_component>());
                jsonPath.append(testDir).append_path("json_serialization_skip_global_transform.json");
                break;

            default:
                OBLO_ASSERT(false);
                break;
            }

            {
                ecs::entity_registry reg;
                reg.init(&typeRegistry);

                ecs_utility::create_named_physical_entity(reg,
                    "A",
                    {},
                    vec3{},
                    quaternion::identity(),
                    vec3::splat(1.f));
                ecs_utility::create_named_physical_entity(reg,
                    "B",
                    {},
                    vec3::splat(1.f),
                    quaternion::identity(),
                    vec3::splat(3.f));

                ecs_utility::create_named_physical_entity<transient_tag>(reg, "C", {}, {}, quaternion::identity(), {});

                data_document doc;
                doc.init();

                ASSERT_TRUE(ecs_serializer::write(doc, doc.get_root(), reg, propertyRegistry, cfg));
                ASSERT_TRUE(json::write(doc, jsonPath));
            }

            {
                data_document doc;

                ASSERT_TRUE(json::read(doc, jsonPath));

                ecs::entity_registry reg;
                reg.init(&typeRegistry);

                ASSERT_TRUE(ecs_serializer::read(reg, doc, doc.get_root(), propertyRegistry));

                auto&& entities = reg.entities();

                ASSERT_EQ(entities.size(), mode == test_mode::skip_transitive ? 2 : 3);

                for (const auto e : entities)
                {
                    const auto& name = reg.get<name_component>(e);
                    const auto& position = reg.get<position_component>(e);
                    const auto& scale = reg.get<scale_component>(e);
                    const auto& rotation = reg.get<rotation_component>(e);
                    const auto* const transform = reg.try_get<global_transform_component>(e);

                    if (name.value == "A")
                    {
                        ASSERT_EQ(position.value, vec3::splat(0.f));
                        ASSERT_EQ(scale.value, vec3::splat(1.f));
                        ASSERT_EQ(rotation.value, quaternion::identity());

                        ASSERT_FALSE(reg.has<transient_tag>(e));
                    }
                    else if (name.value == "B")
                    {
                        ASSERT_EQ(position.value, vec3::splat(1.f));
                        ASSERT_EQ(scale.value, vec3::splat(3.f));
                        ASSERT_EQ(rotation.value, quaternion::identity());

                        ASSERT_FALSE(reg.has<transient_tag>(e));
                    }
                    else if (name.value == "C")
                    {
                        ASSERT_EQ(position.value, vec3::splat(0.f));
                        ASSERT_EQ(scale.value, vec3::splat(0.f));
                        ASSERT_EQ(rotation.value, quaternion::identity());

                        ASSERT_TRUE(reg.has<transient_tag>(e));
                    }
                    else
                    {
                        ASSERT_TRUE(false);
                    }

                    if (mode == test_mode::skip_global_transform)
                    {
                        ASSERT_FALSE(transform);
                    }
                    else
                    {
                        ASSERT_TRUE(transform);
                    }
                }
            }
        }
    }

    namespace
    {
        struct array_test_struct
        {
            u32 u32Val;
            dynamic_array<bool> dBool;

            bool operator==(const array_test_struct&) const = default;
        };

        struct array_test_component
        {
            u32 u4[4];
            array_test_struct s3[3];

            dynamic_array<u32> dU32;
            dynamic_array<dynamic_array<array_test_struct>> dds;

            dynamic_array<dynamic_array<u32>> ddU32;

            bool operator==(const array_test_component&) const = default;
        };
    }

    TEST_F(ecs_serialization_test, json_arrays)
    {
        {
            auto&& r = get_reflection_registrant();

            r.add_class<array_test_struct>()
                .add_field(&array_test_struct::u32Val, "u32Val")
                .add_field(&array_test_struct::dBool, "dBool");

            r.add_class<array_test_component>()
                .add_field(&array_test_component::u4, "u4")
                .add_field(&array_test_component::s3, "s3")
                .add_field(&array_test_component::dU32, "dU32")
                .add_field(&array_test_component::dds, "dds")
                .add_field(&array_test_component::ddU32, "ddU32")
                .add_tag<ecs::component_type_tag>()
                .add_ranged_type_erasure();
        }

        register_from_reflection();

        const auto jsonPath = string_builder{}.append(testDir).append_path("json_arrays.json");

        array_test_component expected;

        {
            expected.u4[0] = 0;
            expected.u4[1] = 1;
            expected.u4[2] = 2;
            expected.u4[3] = 3;

            expected.s3[0].u32Val = 3;
            expected.s3[1].u32Val = 2;
            expected.s3[1].dBool = {true, true, false};
            expected.s3[2].u32Val = 1;

            expected.dU32 = {10, 11};

            {
                auto& ddsE = expected.dds.emplace_back().emplace_back();
                ddsE.u32Val = 42;
            }

            {
                expected.ddU32.emplace_back() = {0, 1, 2};
                expected.ddU32.emplace_back() = {42};
                expected.ddU32.emplace_back() = {9, 8, 7};
            }
        }

        {
            ecs::entity_registry reg;
            reg.init(&typeRegistry);

            const auto e = ecs_utility::create_named_physical_entity(reg,
                "e",
                {},
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            auto& a = reg.add<array_test_component>(e);
            a = expected;

            data_document doc;
            doc.init();

            ASSERT_TRUE(ecs_serializer::write(doc, doc.get_root(), reg, propertyRegistry));
            ASSERT_TRUE(json::write(doc, jsonPath));
        }

        {

            data_document doc;

            ASSERT_TRUE(json::read(doc, jsonPath));

            ecs::entity_registry reg;
            reg.init(&typeRegistry);

            ASSERT_TRUE(ecs_serializer::read(reg, doc, doc.get_root(), propertyRegistry));

            ASSERT_EQ(reg.entities().size(), 1);

            const auto e = reg.entities().front();
            const auto& a = reg.get<array_test_component>(e);

            ASSERT_EQ(a, expected);
        }
    }

    TEST_F(ecs_serialization_test, json_hierarchy)
    {
        register_from_reflection();

        const auto jsonPath = string_builder{}.append(testDir).append_path("json_hierarchy.json").make_absolute_path();

        auto log = finally(
            [&jsonPath, this]
            {
                if (HasFatalFailure())
                {
                    std::cout << "Test failed, files can be found at: " << jsonPath.c_str() << std::endl;
                }
            });

        {
            ecs::entity_registry reg;
            reg.init(&typeRegistry);

            const auto rootEntity1 = ecs_utility::create_named_physical_entity(reg,
                "rootEntity1",
                {},
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            const auto childA1 = ecs_utility::create_named_physical_entity(reg,
                "childA1",
                rootEntity1,
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            const auto childB1 = ecs_utility::create_named_physical_entity(reg,
                "childB1",
                rootEntity1,
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            const auto childAB1 = ecs_utility::create_named_physical_entity(reg,
                "childAB1",
                childB1,
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            const auto rootEntity2 = ecs_utility::create_named_physical_entity(reg,
                "rootEntity2",
                {},
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            const auto rootEntity3 = ecs_utility::create_named_physical_entity(reg,
                "rootEntity3",
                {},
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            const auto childA3 = ecs_utility::create_named_physical_entity(reg,
                "childA3",
                rootEntity3,
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            data_document doc;
            doc.init();

            ASSERT_TRUE(ecs_serializer::write(doc, doc.get_root(), reg, propertyRegistry));
            ASSERT_TRUE(json::write(doc, jsonPath));
        }

        {
            data_document doc;

            ASSERT_TRUE(json::read(doc, jsonPath));

            ecs::entity_registry reg;
            reg.init(&typeRegistry);

            ASSERT_TRUE(ecs_serializer::read(reg, doc, doc.get_root(), propertyRegistry));

            ASSERT_EQ(reg.entities().size(), 7);

            auto findByName = [&reg](const deque<ecs::entity>& range, string_view name) -> ecs::entity
            {
                for (auto e : range)
                {
                    if (auto* nc = reg.try_get<name_component>(e); nc)
                    {
                        if (nc->value == name)
                        {
                            return e;
                        }
                    }
                }
                return {};
            };

            auto expectChildren = [&reg, &findByName](ecs::entity e,
                                      std::initializer_list<string_view> expected,
                                      deque<ecs::entity>& orderedChildren)
            {
                orderedChildren.clear();

                deque<ecs::entity> children;
                ecs_utility::find_children(reg, e, children);

                ASSERT_EQ(children.size(), expected.size());

                for (auto name : expected)
                {
                    const ecs::entity c = findByName(children, name);
                    ASSERT_TRUE(c);

                    orderedChildren.push_back(c);

                    ASSERT_EQ(ecs_utility::find_parent(reg, c), e);
                }
            };

            deque<ecs::entity> roots;
            ecs_utility::find_roots(reg, roots);

            ASSERT_EQ(roots.size(), 3);

            const auto rootEntity1 = findByName(roots, "rootEntity1");
            const auto rootEntity2 = findByName(roots, "rootEntity2");
            const auto rootEntity3 = findByName(roots, "rootEntity3");

            ASSERT_TRUE(rootEntity1);
            ASSERT_TRUE(rootEntity2);
            ASSERT_TRUE(rootEntity3);

            deque<ecs::entity> children;

            expectChildren(rootEntity1, {"childA1", "childB1"}, children);

            const auto childA1 = children[0];
            const auto childB1 = children[1];

            expectChildren(childA1, {}, children);
            expectChildren(childB1, {"childAB1"}, children);

            expectChildren(rootEntity2, {}, children);
            expectChildren(rootEntity3, {"childA3"}, children);
        }
    }
}