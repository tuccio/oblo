oblo_add_library(ui_layout_sandbox MODULE)

target_link_libraries(
    oblo_ui_layout_sandbox
    PRIVATE
    oblo::app
    oblo::ecs
    oblo::graphics
    oblo::input
    oblo::log
    oblo::modules
    oblo::renderer
    oblo::scene::headers
    oblo::ui::core
    oblo::ui::game
)

oblo_add_data_folder(oblo::ui_layout_sandbox::data "${CMAKE_CURRENT_SOURCE_DIR}/resources")

oblo_symlink_to_data_folder(
    oblo_ui_layout_sandbox
    oblo::ui_layout_sandbox::data
    "$<TARGET_FILE_DIR:oblo_ui_layout_sandbox>/ui_layout"
)