oblo_add_library(
    resource MODULE
    NAMESPACE oblo::script
)

oblo_register_core_module(oblo::script::resource)

target_link_libraries(
    oblo_script_resource
    PUBLIC
    oblo::script::runtime
    oblo::resource
    PRIVATE
    oblo::modules
    oblo::properties
    oblo::reflection
)