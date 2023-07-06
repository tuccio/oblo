oblo_add_library(vk)

target_link_libraries(
    oblo_vk
    PUBLIC
    oblo::core
    3rdparty::vulkan-headers
    3rdparty::unordered_dense
    PRIVATE
    3rdparty::vulkan-loader
    3rdparty::vulkan-memory-allocator
    3rdparty::glslang
)