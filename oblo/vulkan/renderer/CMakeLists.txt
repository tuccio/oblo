oblo_add_library(vk_renderer)

target_link_libraries(
    oblo_vk_renderer
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::vk
    PRIVATE
    oblo::resource
    oblo::scene
    efsw::efsw
    spirv-cross::spirv-cross
)

oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan")

target_link_libraries(
    oblo_test_vk_renderer
    PRIVATE
    oblo::sandbox
)

set(_test_resources "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test/resources/renderer")

file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test/resources")
oblo_create_symlink("${CMAKE_CURRENT_SOURCE_DIR}/test/resources" ${_test_resources})

target_compile_definitions(oblo_test_vk_renderer PRIVATE "-DOBLO_TEST_RESOURCES=\"${_test_resources}\"")