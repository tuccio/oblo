
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/print.hpp>
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include "dotnet_bindings.hpp"

int main(int argc, char* argv[])
{
    using namespace oblo;

    if (argc < 2)
    {
        print("Usage: " OBLO_STRINGIZE(OBLO_PROJECT_NAME) " <output file>");
    }

    module_manager mm;

    auto* reflection = mm.load<reflection::reflection_module>();
    mm.load<scene_module>();
    mm.load<graphics_module>();
    mm.load("oblo_dotnet_behaviour");

    if (!mm.finalize())
    {
        return 1;
    }

    property_registry properties;
    properties.init(reflection->get_registry());

    cstring_view outFile = argv[1];

    string_builder outDir;
    filesystem::parent_path(outFile, outDir);

    filesystem::create_directories(outDir).assert_value();

    ecs::type_registry types;

    ecs_utility::register_reflected_component_and_tag_types(reflection->get_registry(), &types, &properties);

    if (!gen::dotnet::generate_bindings(reflection->get_registry(), types, properties, outFile))
    {
        return 1;
    }

    return 0;
}