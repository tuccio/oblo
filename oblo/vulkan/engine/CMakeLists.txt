oblo_add_library(
    engine
    NAMESPACE oblo::vulkan
)

find_package(VulkanHeaders REQUIRED)

target_link_libraries(
    oblo_vulkan_engine
    PUBLIC
    oblo::core
    oblo::modules
    oblo::options
    PRIVATE
    oblo::app
    oblo::gpu::vulkan
    oblo::log
    oblo::renderer
    Vulkan::Headers
)

if(LINUX)
    find_package(SDL2 REQUIRED)

    target_link_libraries(
        oblo_vulkan_engine
        PRIVATE
        SDL2::SDL2
    )
endif()
