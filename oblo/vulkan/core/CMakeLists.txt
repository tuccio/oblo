oblo_add_library(core
    NAMESPACE oblo::vk
)

find_package(VulkanLoader REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

target_link_libraries(
    oblo_vk_core
    PUBLIC
    oblo::core
    oblo::gpu::vulkan
    oblo::vk::compiler
    Vulkan::Headers
    PRIVATE
    oblo::log
    Vulkan::Loader
    GPUOpen::VulkanMemoryAllocator
)