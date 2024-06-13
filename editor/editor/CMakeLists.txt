oblo_add_library(editor)

find_package(imguizmo REQUIRED)

target_link_libraries(
    oblo_editor
    PUBLIC
    oblo::asset
    oblo::core
    oblo::ecs
    PRIVATE
    oblo::graphics
    oblo::input
    oblo::properties
    oblo::scene
    oblo::vk
    imgui::imgui
    iconfontcppheaders::iconfontcppheaders
    imguizmo::imguizmo
)