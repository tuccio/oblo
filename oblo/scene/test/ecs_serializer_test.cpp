#include <gtest/gtest.h>

#include <oblo/core/filesystem/filesystem.hpp>
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
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>
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

            void register_reflected_component_types()
            {
                ecs_utility::register_reflected_component_types(reflectionRegistry, &typeRegistry, &propertyRegistry);
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
        register_reflected_component_types();

        const auto jsonPath = string_builder{}.append(testDir).append_path("json_serialization.json");

        {
            ecs::entity_registry reg;
            reg.init(&typeRegistry);

            ecs_utility::create_named_physical_entity(reg, "A", vec3{}, quaternion::identity(), vec3::splat(1.f));
            ecs_utility::create_named_physical_entity(reg,
                "B",
                vec3::splat(1.f),
                quaternion::identity(),
                vec3::splat(3.f));

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

            for (const auto e : reg.entities())
            {
                const auto& name = reg.get<name_component>(e);
                const auto& position = reg.get<position_component>(e);
                const auto& scale = reg.get<scale_component>(e);
                const auto& rotation = reg.get<rotation_component>(e);

                if (name.value == "A")
                {
                    ASSERT_EQ(position.value, vec3::splat(0.f));
                    ASSERT_EQ(scale.value, vec3::splat(1.f));
                    ASSERT_EQ(rotation.value, quaternion::identity());
                }
                else if (name.value == "B")
                {
                    ASSERT_EQ(position.value, vec3::splat(1.f));
                    ASSERT_EQ(scale.value, vec3::splat(3.f));
                    ASSERT_EQ(rotation.value, quaternion::identity());
                }
                else
                {
                    ASSERT_TRUE(false);
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
        };

        struct array_test_component
        {
            u32 u4[4];
            array_test_struct s3[3];

            dynamic_array<u32> dU32;
            dynamic_array<dynamic_array<array_test_struct>> dds;

            dynamic_array<dynamic_array<u32>> ddU32;
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

        register_reflected_component_types();

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

            const auto e =
                ecs_utility::create_named_physical_entity(reg, "e", vec3{}, quaternion::identity(), vec3::splat(1.f));

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
        }
    }
}