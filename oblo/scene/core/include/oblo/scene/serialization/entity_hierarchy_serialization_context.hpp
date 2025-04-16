#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>

namespace oblo
{
    class entity_hierarchy_serialization_context
    {
    public:
        SCENE_API expected<> init();

        SCENE_API const property_registry& get_property_registry() const;
        SCENE_API const ecs::type_registry& get_type_registry() const;

        SCENE_API ecs_serializer::write_config make_write_config() const;
        SCENE_API ecs_serializer::read_config make_read_config() const;

    private:
        const property_registry* m_properties{};
        const ecs::type_registry* m_types{};
        ecs::component_and_tag_sets m_transientTypes{};
    };
}