oblo_add_library(reflection)

target_link_libraries(
    oblo_reflection
    PUBLIC
    oblo::core
    oblo::modules
    PRIVATE
    oblo::ecs
)