oblo_add_library(importers MODULE)

target_link_libraries(
    oblo_importers
    PUBLIC
    oblo::asset
    oblo::modules
    oblo::scene
    PRIVATE
    oblo::runtime
    TinyGLTF::TinyGLTF
    stb::stb
    Vulkan::Headers
)

target_link_libraries(
    oblo_test_importers
    PRIVATE
    oblo::test::glTF::SampleModels
)