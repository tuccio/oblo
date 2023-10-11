oblo_add_library(vk_renderer)

target_link_libraries(
    oblo_vk_renderer
    PUBLIC
    oblo::core
    oblo::vk
)

oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan")

target_link_libraries(
    oblo_test_vk_renderer
    PRIVATE
    oblo::sandbox
)