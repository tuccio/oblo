oblo_add_library(dotnet_bindings MODULE)

target_link_libraries(
    oblo_dotnet_bindings
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::properties
    oblo::reflection
    oblo::resource
)

add_library(oblo::dotnet::bindings ALIAS oblo_dotnet_bindings)