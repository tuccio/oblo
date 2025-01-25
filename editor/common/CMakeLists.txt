oblo_add_library(editor_common)

target_link_libraries(
    oblo_editor_common
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::editor_core
    imgui::imgui
    PRIVATE
    iconfontcppheaders::iconfontcppheaders
)

add_library(oblo::editor::common ALIAS oblo_editor_common)
