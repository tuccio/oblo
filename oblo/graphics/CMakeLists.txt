oblo_add_library(graphics)

target_link_libraries(
    oblo_graphics
    PUBLIC
    oblo::core
    oblo::resource
    oblo::vk::renderer
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::options
    oblo::reflection
    oblo::scene
    oblo::vk::core
    oblo::vk::engine
)