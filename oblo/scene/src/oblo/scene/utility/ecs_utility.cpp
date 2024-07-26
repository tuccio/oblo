#include <oblo/scene/utility/ecs_utility.hpp>

#include <oblo/core/log.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/math/transform.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/reflection/concepts/ranged_type_erasure.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/name_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
}

namespace oblo::ecs_utility
{
    void register_reflected_component_types(const reflection::reflection_registry& reflection,
        ecs::type_registry* typeRegistry,
        property_registry* propertyRegistry)
    {
        dynamic_array<reflection::type_handle> componentTypes;
        reflection.find_by_tag<ecs::component_type_tag>(componentTypes);

        if (typeRegistry)
        {
            for (const auto typeHandle : componentTypes)
            {
                const auto typeData = reflection.get_type_data(typeHandle);

                if (typeRegistry->find_component(typeData.type))
                {
                    continue;
                }

                const auto rte = reflection.find_concept<reflection::ranged_type_erasure>(typeHandle);

                if (rte)
                {
                    const ecs::component_type_desc desc{
                        .type = typeData.type,
                        .size = typeData.size,
                        .alignment = typeData.alignment,
                        .create = rte->create,
                        .destroy = rte->destroy,
                        .move = rte->move,
                        .moveAssign = rte->moveAssign,
                    };

                    if (!typeRegistry->register_component(desc))
                    {
                        log::error("Failed to register component {}", typeData.type.name);
                    }
                }
            }
        }

        if (propertyRegistry)
        {
            for (const auto typeHandle : componentTypes)
            {
                const auto typeData = reflection.get_type_data(typeHandle);
                propertyRegistry->build_from_reflection(typeData.type);
            }
        }
    }

    ecs::entity create_named_physical_entity(ecs::entity_registry& registry,
        const ecs::component_and_tag_sets& extraComponentsOrTags,
        std::string_view name,
        const vec3& position,
        const quaternion& rotation,
        const vec3& scale)
    {
        const auto builtIn = ecs::make_type_sets<name_component,
            position_component,
            rotation_component,
            scale_component,
            global_transform_component>(registry.get_type_registry());

        auto typeSets = extraComponentsOrTags;
        typeSets.components.add(builtIn.components);

        const auto e = registry.create(typeSets);

        registry.get<name_component>(e).value = name;
        registry.get<position_component>(e).value = position;
        registry.get<rotation_component>(e).value = rotation;
        registry.get<scale_component>(e).value = scale;
        registry.get<global_transform_component>(e).localToWorld = make_transform_matrix(position, rotation, scale);

        return e;
    }
}