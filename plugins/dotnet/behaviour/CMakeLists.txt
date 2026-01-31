oblo_add_library(
    behaviour MODULE
    NAMESPACE oblo::dotnet
)

oblo_register_core_module(oblo::dotnet::behaviour)

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