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

option(OBLO_GLTF_SAMPLE_MODELS "Path to a reference of https://github.com/KhronosGroup/glTF-Sample-Models (based on commit 4ca06672ce15d6a27bfb5cf14459bc52fd9044d1)" "")

target_compile_definitions(
    oblo_test_importers
    PRIVATE
    "-DOBLO_GLTF_SAMPLE_MODELS=\"${OBLO_GLTF_SAMPLE_MODELS}\""
)