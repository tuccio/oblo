oblo_add_library(properties)

target_link_libraries(
    oblo_properties
    PUBLIC
    oblo::core
    oblo::reflection
)