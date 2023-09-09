oblo_add_library(runtime NAMESPACE editor)

target_link_libraries(
    oblo_editor_runtime
    PUBLIC
    oblo::core
)