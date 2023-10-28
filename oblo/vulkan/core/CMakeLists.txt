oblo_add_library(vk)

target_link_libraries(
    oblo_vk
    PUBLIC
    oblo::core
    Vulkan::Headers
    PRIVATE
    Vulkan::Vulkan
    vulkan-memory-allocator::vulkan-memory-allocator
    glslang::glslang
    glslang::SPIRV
)