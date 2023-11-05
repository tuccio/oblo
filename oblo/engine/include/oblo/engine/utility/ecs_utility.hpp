#pragma once

namespace oblo::ecs
{
    class type_registry;
}

namespace oblo::reflection
{
    class reflection_registry;
}

namespace oblo::engine::ecs_utility
{
    ENGINE_API void register_reflected_component_types(ecs::type_registry& typeRegistry,
        const reflection::reflection_registry& reflection);
}