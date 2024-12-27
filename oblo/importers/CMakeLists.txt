oblo_add_library(importers MODULE)

find_package(meshoptimizer REQUIRED)

target_link_libraries(
    oblo_importers
    PUBLIC
    oblo::asset
    oblo::modules
    oblo::scene
    PRIVATE
    oblo::log
    oblo::runtime
    oblo::thread
    meshoptimizer::meshoptimizer
    TinyGLTF::TinyGLTF
    stb::stb
    Vulkan::Headers
)

target_link_libraries(
    oblo_test_importers
    PRIVATE
    oblo::test::glTF::SampleModels
)