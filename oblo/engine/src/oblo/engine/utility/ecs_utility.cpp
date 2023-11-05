#include <oblo/engine/utility/ecs_utility.hpp>

#include <oblo/core/log.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/reflection/concepts/ranged_type_erasure.hpp>
#include <oblo/reflection/reflection_registry.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
}

namespace oblo::ecs_utility
{
    void register_reflected_component_types(ecs::type_registry& typeRegistry,
        const reflection::reflection_registry& reflection)
    {
        std::vector<reflection::type_handle> componentTypes;
        reflection.find_by_tag<ecs::component_type_tag>(componentTypes);

        for (const auto typeHandle : componentTypes)
        {
            const auto typeData = reflection.get_type_data(typeHandle);

            if (typeRegistry.find_component(typeData.type))
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

                if (!typeRegistry.register_component(desc))
                {
                    log::error("Failed to register component {}", typeData.type.name);
                }
            }
        }
    }
}