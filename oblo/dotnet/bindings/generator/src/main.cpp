
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include "dotnet_bindings.hpp"

int main(int, char*[])
{
    using namespace oblo;

    module_manager mm;

    auto* reflection = mm.load<reflection::reflection_module>();
    mm.load<scene_module>();
    mm.load<graphics_module>();

    if (!mm.finalize())
    {
        return 1;
    }

    property_registry properties;
    properties.init(reflection->get_registry());

    ecs::type_registry types;

    ecs_utility::register_reflected_component_and_tag_types(reflection->get_registry(), &types, &properties);

    if (!gen::dotnet::generate_bindings(reflection->get_registry(),
            types,
            properties,
            "./dotnet_bindings.gen.cpp",
            "./DotNet.Bindings.Gen.cs"))
    {
        return 1;
    }

    return 0;
}