oblo_add_library(script_resource MODULE)

oblo_register_core_module(script_resource)

target_link_libraries(
    oblo_script_resource
    PUBLIC
    oblo::script::runtime
    PRIVATE
    oblo::modules
    oblo::reflection
    oblo::resource
)

add_library(oblo::script::resource ALIAS oblo_script_resource)