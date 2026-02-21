oblo_add_library(
    engine
    NAMESPACE oblo::vk
)

find_package(VulkanHeaders REQUIRED)

target_link_libraries(
    oblo_vk_engine
    PUBLIC
    oblo::core
    oblo::modules
    oblo::options
    PRIVATE
    oblo::app
    oblo::log
    oblo::renderer
    oblo::vk::core
    Vulkan::Headers
)