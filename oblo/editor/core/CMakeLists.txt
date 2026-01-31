oblo_add_library(
    core
    NAMESPACE oblo::editor
)

target_link_libraries(
    oblo_editor_core
    INTERFACE
    oblo::asset
    PUBLIC
    oblo::modules
    oblo::core
)