oblo_add_library(
    bindings MODULE
    NAMESPACE oblo::dotnet
)

target_link_libraries(
    oblo_dotnet_bindings
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::properties
    oblo::reflection
    oblo::resource
    oblo::scene
)