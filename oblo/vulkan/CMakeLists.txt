oblo_add_library(vk)

target_link_libraries(
    oblo_vk
    PUBLIC
    oblo::core
    3rdparty::vulkan-headers
    PRIVATE
    3rdparty::vulkan-loader
    3rdparty::vulkan-memory-allocator
    3rdparty::glslang
)

target_link_libraries(
    oblo_test_vk
    PRIVATE
    3rdparty::sdl
    3rdparty::imgui_sdl_vk
)