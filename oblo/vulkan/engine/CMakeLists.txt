oblo_add_library(vk_engine)

find_package(Vulkan REQUIRED)

target_link_libraries(
    oblo_vk_engine
    PUBLIC
    oblo::core
    oblo::modules
    PRIVATE
    oblo::vk
    oblo::vk_renderer
    oblo::app
    Vulkan::Vulkan
)