oblo_add_library(graphics)

target_link_libraries(
    oblo_graphics
    PUBLIC
    oblo::core
    oblo::renderer
    oblo::resource
    PRIVATE
    oblo::ecs
    oblo::log
    oblo::options
    oblo::reflection
    oblo::scene
    oblo::vulkan::engine
)