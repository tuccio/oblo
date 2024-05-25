oblo_add_library(smoke TEST_MAIN)

target_link_libraries(
    oblo_smoke
    PUBLIC
    oblo::asset
    oblo::ecs
    oblo::resource
    oblo::runtime
    oblo::sandbox
    PRIVATE
    oblo::renderdoc
    imgui::imgui
)

target_link_libraries(
    oblo_test_smoke
    PRIVATE
    oblo::importers
    oblo::modules
    oblo::scene
    oblo::test::glTF::SampleModels
)

# We need access to shaders etc
set_target_properties(oblo_test_smoke PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")