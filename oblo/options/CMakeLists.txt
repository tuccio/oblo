oblo_add_library(options)

target_link_libraries(
    oblo_options
    PUBLIC
    oblo::core
    oblo::properties
    oblo::modules
)