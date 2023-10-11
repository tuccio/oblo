oblo_add_library(graphics)

target_link_libraries(
    oblo_graphics
    PUBLIC
    oblo::vk
    oblo::vk_renderer
    PRIVATE
    oblo::ecs
)