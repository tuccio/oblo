#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/ecs/type_set.hpp>

namespace oblo
{
    class property_registry;

    struct quaternion;
    struct vec3;
}

namespace oblo::ecs
{
    class entity_registry;
    class type_registry;
    struct component_and_tag_sets;
}

namespace oblo::reflection
{
    class reflection_registry;
}

namespace oblo::ecs_utility
{
    SCENE_API void register_reflected_component_types(const reflection::reflection_registry& reflection,
        ecs::type_registry* typeRegistry,
        property_registry* propertyRegistry);

    SCENE_API ecs::entity create_named_physical_entity(ecs::entity_registry& registry,
        const ecs::component_and_tag_sets& extraComponentsOrTags,
        string_view name,
        const vec3& position,
        const quaternion& rotation,
        const vec3& scale);

    template <typename... ComponentsOrTags>
    ecs::entity create_named_physical_entity(ecs::entity_registry& registry,
        string_view name,
        const vec3& position,
        const quaternion& rotation,
        const vec3& scale)
    {
        return create_named_physical_entity(registry,
            ecs::make_type_sets<ComponentsOrTags...>(registry.get_type_registry()),
            name,
            position,
            rotation,
            scale);
    }
}