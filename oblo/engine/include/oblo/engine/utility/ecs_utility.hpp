#pragma once

namespace oblo
{
    class property_registry;
}

namespace oblo::ecs
{
    class type_registry;
}

namespace oblo::reflection
{
    class reflection_registry;
}

namespace oblo::ecs_utility
{
    ENGINE_API void register_reflected_component_types(const reflection::reflection_registry& reflection,
        ecs::type_registry* typeRegistry,
        property_registry* propertyRegistry);
}