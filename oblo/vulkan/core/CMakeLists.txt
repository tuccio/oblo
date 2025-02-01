oblo_add_library(vk)

find_package(Vulkan REQUIRED)
find_package(vulkan-memory-allocator REQUIRED)

target_link_libraries(
    oblo_vk
    PUBLIC
    oblo::core
    oblo::vk_compiler
    Vulkan::Headers
    PRIVATE
    oblo::log
    Vulkan::Vulkan
    vulkan-memory-allocator::vulkan-memory-allocator
)