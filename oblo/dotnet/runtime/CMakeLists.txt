oblo_add_library(dotnet_rt MODULE)

oblo_register_core_module(dotnet_rt)

find_package(utf8cpp REQUIRED)

target_link_libraries(
    oblo_dotnet_rt
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::reflection
    oblo::resource
    utf8cpp
)

add_library(oblo::dotnet::runtime ALIAS oblo_dotnet_rt)

add_dependencies(oblo_dotnet_rt oblo::dotnet::bindings Oblo.Managed-build)