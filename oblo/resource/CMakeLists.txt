oblo_add_library(resource)

target_link_libraries(
    oblo_resource
    PUBLIC
    oblo::core
    PRIVATE
    oblo::thread
    nlohmann_json::nlohmann_json
)