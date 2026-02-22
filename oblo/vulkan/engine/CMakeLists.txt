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
    oblo::log
    oblo::renderer
    oblo::vulkan::core
    Vulkan::Headers
)