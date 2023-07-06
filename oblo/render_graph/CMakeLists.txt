oblo_add_library(render_graph)

target_link_libraries(
    oblo_render_graph
    PUBLIC
    oblo::core
    oblo::vk
)