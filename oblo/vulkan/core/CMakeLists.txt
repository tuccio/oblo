oblo_add_library(core
    NAMESPACE oblo::vulkan
)

find_package(VulkanLoader REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

target_link_libraries(
    oblo_vulkan_core
    PUBLIC
    oblo::core
    oblo::gpu::vulkan
    oblo::vulkan::compiler
    Vulkan::Headers
    PRIVATE
    oblo::log
    Vulkan::Loader
    GPUOpen::VulkanMemoryAllocator
)