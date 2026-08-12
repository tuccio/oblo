oblo_add_library(
    windows
    NAMESPACE oblo::editor
)

find_package(iconfontcppheaders REQUIRED)
find_package(ImGui REQUIRED)

target_link_libraries(
    oblo_editor_windows
    PUBLIC
    oblo::asset
    oblo::core
    oblo::ecs
    oblo::editor::core
    oblo::modules
    PRIVATE
    oblo::editor::common
    oblo::cpurt
    oblo::graphics
    oblo::input
    oblo::options
    oblo::properties
    oblo::renderer
    oblo::scene
    imgui::imgui
    iconfontcppheaders::iconfontcppheaders
)
