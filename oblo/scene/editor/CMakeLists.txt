oblo_add_library(scene_editor)

find_package(iconfontcppheaders REQUIRED)
find_package(ImGui REQUIRED)

target_link_libraries(
    oblo_scene_editor
    PUBLIC
    oblo::asset
    oblo::modules
    oblo::runtime
    oblo::scene
    PRIVATE
    oblo::editor::core
    oblo::editor::common
    oblo::editor::windows
    oblo::graphics
    imgui::imgui
    iconfontcppheaders::iconfontcppheaders
)

add_library(oblo::scene::editor ALIAS oblo_scene_editor)
