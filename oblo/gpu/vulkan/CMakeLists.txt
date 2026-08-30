oblo_add_library(
    vulkan
    NAMESPACE oblo::gpu
)

find_package(VulkanLoader REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

target_link_libraries(
    oblo_gpu_vulkan
    PUBLIC
    oblo::core
    oblo::gpu::core
    Vulkan::Headers
    PRIVATE
    Vulkan::Loader
    GPUOpen::VulkanMemoryAllocator
)

if(LINUX)
    find_package(SDL2 REQUIRED)

    target_link_libraries(
        oblo_gpu_vulkan
        PRIVATE
        SDL2::SDL2
    )
endif()