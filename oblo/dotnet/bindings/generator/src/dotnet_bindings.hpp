#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>

namespace oblo
{
    class property_registry;
}

namespace oblo::ecs
{
    class entity_registry;
    class type_registry;
}

namespace oblo::reflection
{
    class reflection_registry;
}

namespace oblo::gen::dotnet
{
    expected<> generate_bindings(const reflection::reflection_registry& reflectionRegistry,
        const ecs::type_registry& typeRegistry,
        const property_registry& propertyRegistry,
        cstring_view nativePath,
        cstring_view managedPath);
}