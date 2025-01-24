oblo_add_library(scene_editor MODULE)

target_link_libraries(
    oblo_scene_editor
    PUBLIC
    oblo::asset
    oblo::modules
    oblo::scene
)