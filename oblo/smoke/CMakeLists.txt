oblo_add_library(smoke TEST_MAIN)

find_package(ImGui REQUIRED)

target_link_libraries(
    oblo_smoke
    PUBLIC
    oblo::asset
    oblo::ecs
    oblo::importers
    oblo::options
    oblo::resource
    oblo::runtime
    oblo::sandbox
    oblo::scene
    oblo::thread
    PRIVATE
    oblo::renderdoc
    imgui::imgui
)

target_link_libraries(
    oblo_test_smoke
    PRIVATE
    oblo::modules
    oblo::test::glTF::SampleModels
)

# We need access to shaders etc
set_target_properties(oblo_test_smoke PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")