oblo_add_library(graphics)

target_link_libraries(
    oblo_graphics
    PUBLIC
    oblo::vk
    PRIVATE
    oblo::ecs
)