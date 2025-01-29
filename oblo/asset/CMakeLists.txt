oblo_add_library(asset)

target_link_libraries(
    oblo_asset
    PUBLIC
    oblo::core
    oblo::log
    oblo::properties
    oblo::resource
    PRIVATE
    oblo::thread
    nlohmann_json::nlohmann_json
)

target_link_libraries(
    oblo_test_asset
    PRIVATE
    oblo::thread
)