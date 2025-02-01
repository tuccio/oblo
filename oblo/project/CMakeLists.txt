oblo_add_library(project)

target_link_libraries(
    oblo_project
    PUBLIC
    oblo::core
    PRIVATE
    oblo::properties
)