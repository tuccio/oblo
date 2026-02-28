oblo_add_library(
    behaviour MODULE
    NAMESPACE oblo::script
)

oblo_register_core_module(oblo::script::behaviour)

target_link_libraries(
    oblo_script_behaviour
    PUBLIC
    oblo::script::resource
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::modules
    oblo::options
    oblo::properties
    oblo::reflection
    oblo::resource
    oblo::scene::headers
    oblo::script::runtime
)