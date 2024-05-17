oblo_add_test(smoke MAIN)

target_link_libraries(
    oblo_test_smoke
    PRIVATE
    oblo::asset
    oblo::ecs
    oblo::importers
    oblo::resource
    oblo::runtime
    oblo::sandbox
)