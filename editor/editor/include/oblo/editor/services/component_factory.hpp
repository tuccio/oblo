#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/ecs/handles.hpp>

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor
{
    using component_initializer = void (*)(byte* component);

    class component_factory
    {
    public:
        bool add(ecs::entity_registry& registry, ecs::entity entity, ecs::component_type component) const;

        void register_initializer(ecs::component_type component, component_initializer initializer);

    private:
        flat_dense_map<ecs::component_type, component_initializer> m_initializers;
    };
}