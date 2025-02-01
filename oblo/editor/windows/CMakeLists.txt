oblo_add_library(editor_windows)

find_package(iconfontcppheaders REQUIRED)
find_package(ImGui REQUIRED)
find_package(imguizmo REQUIRED)

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
    oblo::graphics
    oblo::input
    oblo::options
    oblo::properties
    oblo::scene
    oblo::vk
    oblo::vk_renderer
    imgui::imgui
    iconfontcppheaders::iconfontcppheaders
    imguizmo::imguizmo
)

add_library(oblo::editor::windows ALIAS oblo_editor_windows)
