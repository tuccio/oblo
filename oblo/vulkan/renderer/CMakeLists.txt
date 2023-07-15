oblo_add_library(vk_renderer)

target_link_libraries(
    oblo_vk_renderer
    PUBLIC
    oblo::core
    oblo::vk
    oblo::render_graph
)

oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan")