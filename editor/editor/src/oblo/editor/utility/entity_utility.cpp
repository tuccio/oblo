#include <oblo/editor/utility/entity_utility.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/scene/components/name_component.hpp>

namespace oblo::editor::entity_utility
{
    const char* get_name_cstr(ecs::entity_registry& reg, ecs::entity e)
    {
        auto* const nameComponent = reg.try_get<name_component>(e);
        return nameComponent && !nameComponent->value.empty() ? nameComponent->value.c_str() : "Unnamed Entity";
    }
}