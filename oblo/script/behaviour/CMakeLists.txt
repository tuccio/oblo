oblo_add_library(script_behaviour MODULE)

oblo_register_core_module(script_behaviour)

target_link_libraries(
    oblo_script_behaviour
    PUBLIC
    oblo::script::resource
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::properties
    oblo::reflection
    oblo::resource
    oblo::script::runtime
)