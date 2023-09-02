oblo_add_library(asset)

target_link_libraries(
    oblo_asset
    PUBLIC
    oblo::core
    PRIVATE
    nlohmann_json::nlohmann_json
)