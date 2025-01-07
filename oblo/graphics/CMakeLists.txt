oblo_add_library(graphics)

target_link_libraries(
    oblo_graphics
    PUBLIC
    oblo::core
    oblo::resource
    PRIVATE
    oblo::ecs
    oblo::options
    oblo::scene
    oblo::reflection
    oblo::vk
    oblo::vk_renderer
)