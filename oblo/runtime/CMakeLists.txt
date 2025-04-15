oblo_add_library(runtime MODULE)

oblo_register_core_module_loader(runtime)

target_link_libraries(
    oblo_runtime
    PUBLIC
    oblo::ecs
    oblo::graphics
    oblo::modules
    oblo::properties
    oblo::reflection
    oblo::scene
)