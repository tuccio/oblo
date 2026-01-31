oblo_add_library(
    renderer
    NAMESPACE oblo::vk
)

find_package(spirv-cross REQUIRED)

target_link_libraries(
    oblo_vk_renderer
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::metrics
    oblo::options
    oblo::vk::core
    PRIVATE
    oblo::log
    oblo::resource
    oblo::scene
    spirv-cross::spirv-cross
)

oblo_add_data_folder(oblo::vulkan::shaders "${CMAKE_CURRENT_SOURCE_DIR}/resources")

target_link_libraries(
    oblo_vk_test_renderer
    PRIVATE
    oblo::app
    oblo::resource
    oblo::modules
    oblo::vk::engine
)

set(_test_resources "${CMAKE_CURRENT_SOURCE_DIR}/test/resources")
target_compile_definitions(oblo_vk_test_renderer PRIVATE "-DOBLO_TEST_RESOURCES=\"${_test_resources}\"")