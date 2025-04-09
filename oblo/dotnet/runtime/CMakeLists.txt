oblo_add_library(dotnet_rt MODULE)

oblo_register_core_module(dotnet_rt)

target_link_libraries(
    oblo_dotnet_rt
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::reflection
    oblo::resource
)

add_library(oblo::dotnet::runtime ALIAS oblo_dotnet_rt)