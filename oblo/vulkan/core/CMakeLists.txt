oblo_add_library(vk)

find_package(VulkanLoader REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

target_link_libraries(
    oblo_vk
    PUBLIC
    oblo::core
    oblo::vk_compiler
    Vulkan::Headers
    PRIVATE
    oblo::log
    Vulkan::Loader
    GPUOpen::VulkanMemoryAllocator
)