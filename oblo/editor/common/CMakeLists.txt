oblo_add_library(
    common
    NAMESPACE oblo::editor
)

find_package(iconfontcppheaders REQUIRED)
find_package(ImGui REQUIRED)

target_link_libraries(
    oblo_editor_common
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::editor::core
    oblo::imgui_app
    imgui::imgui
    PRIVATE
    iconfontcppheaders::iconfontcppheaders
)