oblo_add_library(properties)

find_package(rapidjson REQUIRED)

target_link_libraries(
    oblo_properties
    PUBLIC
    oblo::core
    oblo::reflection
    PRIVATE
    oblo::log
    rapidjson
)