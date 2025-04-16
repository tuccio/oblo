oblo_add_library(smoke TEST_MAIN)

target_link_libraries(
    oblo_smoke
    PUBLIC
    oblo::asset
    oblo::ecs
    oblo::options
    oblo::resource
    oblo::runtime
    oblo::scene
    oblo::thread
    PRIVATE
    oblo::app
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

oblo_register_asset_module_loader(oblo_smoke)

if(OBLO_WITH_DOTNET)
    target_link_libraries(
        oblo_test_smoke
        PRIVATE
        oblo::dotnet::asset
        oblo::dotnet::behaviour
    )

    target_compile_definitions(oblo_test_smoke PRIVATE OBLO_SMOKE_DOTNET_TESTS)
endif()