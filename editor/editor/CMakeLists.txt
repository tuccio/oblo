oblo_add_library(editor)

target_link_libraries(
    oblo_editor
    PUBLIC
    oblo::asset
    oblo::core
    oblo::ecs
    PRIVATE
    oblo::engine
    oblo::graphics
    imgui::imgui
    oblo::vk
)