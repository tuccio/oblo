oblo_add_executable(ui_layout_sandbox)

target_link_libraries(
    ui_layout_sandbox
    PRIVATE
    oblo::app
    oblo::input
    oblo::log
    oblo::modules
    oblo::options
    oblo::renderer
    oblo::ui::core
    oblo::vulkan::engine
)

oblo_add_data_folder(oblo::ui_layout_sandbox::data "${CMAKE_CURRENT_SOURCE_DIR}/resources")

oblo_symlink_to_data_folder(
    ui_layout_sandbox
    oblo::ui_layout_sandbox::data
    "$<TARGET_FILE_DIR:ui_layout_sandbox>/ui_layout"
)
