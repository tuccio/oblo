oblo_add_library(vk_engine)

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
    oblo::vk
    oblo::vk_renderer
    Vulkan::Headers
)