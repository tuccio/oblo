oblo_add_library(editor_core)

target_link_libraries(
    oblo_editor_core
    INTERFACE
    oblo::asset
    PUBLIC
    oblo::modules
    oblo::core
)