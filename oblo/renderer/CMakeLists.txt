oblo_add_library(renderer)

find_package(spirv-cross REQUIRED)

target_link_libraries(
    oblo_renderer
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::gpu::core
    oblo::metrics
    oblo::options
    PRIVATE
    oblo::log
    oblo::gpu::vulkan
    oblo::vulkan::compiler
    oblo::resource
    oblo::scene
    spirv-cross::spirv-cross
)

oblo_add_data_folder(oblo::renderer::vulkan_shaders "${CMAKE_CURRENT_SOURCE_DIR}/resources")

target_link_libraries(
    oblo_test_renderer
    PRIVATE
    oblo::app
    oblo::resource
    oblo::modules
    oblo::vulkan::engine
)