oblo_add_library(smoke TEST_MAIN)

target_link_libraries(
    oblo_smoke
    PUBLIC
    oblo::app
    oblo::asset
    oblo::ecs
    oblo::importers
    oblo::options
    oblo::resource
    oblo::runtime
    oblo::scene
    oblo::thread
    PRIVATE
    oblo::renderdoc
)

target_link_libraries(
    oblo_test_smoke
    PRIVATE
    oblo::modules
    oblo::test::glTF::SampleModels
)

# We need access to shaders etc
set_target_properties(oblo_test_smoke PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")