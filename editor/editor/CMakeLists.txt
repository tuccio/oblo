oblo_add_library(editor)

target_link_libraries(
    oblo_editor
    PUBLIC
    oblo::asset
    oblo::core
    PRIVATE
    3rdparty::imgui
)