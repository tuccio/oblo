oblo_add_library(editor_common)

find_package(iconfontcppheaders REQUIRED)
find_package(ImGui REQUIRED)

target_link_libraries(
    oblo_editor_common
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::editor_core
    oblo::imgui_app
    imgui::imgui
    PRIVATE
    iconfontcppheaders::iconfontcppheaders
)

add_library(oblo::editor::common ALIAS oblo_editor_common)
