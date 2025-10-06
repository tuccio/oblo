# GLTF Sample models

add_library(oblo_test_gltf_sample_models INTERFACE)

target_compile_definitions(
    oblo_test_gltf_sample_models
    INTERFACE
    "-DOBLO_GLTF_SAMPLE_MODELS=\"${CMAKE_CURRENT_SOURCE_DIR}/glTF-Sample-Assets\""
)

add_library(oblo::test::glTF::SampleModels ALIAS oblo_test_gltf_sample_models)

# Test assets from oblo-assets subrepo

add_library(oblo_test_assets INTERFACE)

target_compile_definitions(
    oblo_test_assets
    INTERFACE
    "-DOBLO_TEST_ASSETS_DIR=\"${CMAKE_SOURCE_DIR}/data/test\""
)

add_library(oblo::test::assets ALIAS oblo_test_assets)