oblo_add_library(dotnet_behaviour MODULE)

oblo_register_core_module(dotnet_behaviour)

target_link_libraries(
    oblo_dotnet_behaviour
    PRIVATE
    oblo::dotnet::runtime
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::reflection
    oblo::resource
)

add_library(oblo::dotnet::behaviour ALIAS oblo_dotnet_behaviour)
