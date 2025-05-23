oblo_add_library(importers MODULE)

find_package(assimp REQUIRED)
find_package(meshoptimizer REQUIRED)
find_package(stb REQUIRED)
find_package(tinygltf REQUIRED)

target_link_libraries(
    oblo_importers
    PUBLIC
    oblo::asset
    oblo::modules
    oblo::scene
    PRIVATE
    oblo::graphics
    oblo::log
    oblo::thread
    assimp::assimp
    meshoptimizer::meshoptimizer
    TinyGLTF::Defines
    TinyGLTF::TinyGLTF
    stb::stb
)

target_link_libraries(
    oblo_test_importers
    PRIVATE
    oblo::graphics
    oblo::thread
    oblo::test::glTF::SampleModels
)

oblo_register_asset_module(importers)