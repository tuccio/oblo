oblo_add_library(components NAMESPACE editor)

target_link_libraries(
    oblo_editor_components
    PUBLIC
    oblo::editor::runtime
    PRIVATE
    3rdparty::imgui
)