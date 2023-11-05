oblo_add_library(engine MODULE)

target_link_libraries(
    oblo_engine
    PUBLIC
    oblo::asset
    oblo::ecs
    oblo::modules
    oblo::properties
    oblo::reflection
    oblo::resource
)