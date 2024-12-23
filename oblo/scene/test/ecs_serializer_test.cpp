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
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

namespace oblo
{
    namespace
    {
        void clear_directory(cstring_view path)
        {
            filesystem::remove_all(path).assert_value();
        }
    }

    TEST(ecs_serializer, json_serialization)
    {
        module_manager mm;
        mm.load<scene_module>();

        constexpr cstring_view testDir{"./test/ecs_serializer/"};
        clear_directory(testDir);

        filesystem::create_directories(testDir).assert_value();

        const auto& reflectionRegistry = mm.get().load<reflection::reflection_module>()->get_registry();

        property_registry propertyRegistry;
        propertyRegistry.init(reflectionRegistry);

        ecs::type_registry typeRegistry;

        ecs_utility::register_reflected_component_types(reflectionRegistry, &typeRegistry, &propertyRegistry);

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
        }
    }
}