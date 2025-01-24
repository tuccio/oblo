oblo_add_library(scene_editor)

target_link_libraries(
    oblo_scene_editor
    PUBLIC
    oblo::asset
    oblo::modules
    oblo::scene
    PRIVATE
    oblo::editor::core
    imgui::imgui
    iconfontcppheaders::iconfontcppheaders
)

add_library(oblo::scene::editor ALIAS oblo_scene_editor)
