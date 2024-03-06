#include <oblo/editor/services/component_factory.hpp>

#include <oblo/ecs/entity_registry.hpp>

namespace oblo::editor
{
    bool component_factory::add(ecs::entity_registry& registry, ecs::entity entity, ecs::component_type component) const
    {
        ecs::component_and_tag_sets types{};
        types.components.add(component);

        if (registry.try_get(entity, component))
        {
            return false;
        }

        registry.add(entity, types);

        if (const auto initializer = m_initializers.try_find(component))
        {
            (*initializer)(registry.try_get(entity, component));
        }

        return true;
    }

    void component_factory::register_initializer(ecs::component_type component, component_initializer initializer)
    {
        m_initializers.emplace(component, initializer);
    }
}