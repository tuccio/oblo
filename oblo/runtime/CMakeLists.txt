oblo_add_library(runtime MODULE)

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