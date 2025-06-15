oblo_add_library(script_editor MODULE)

oblo_register_editor_module(script_editor)

target_link_libraries(
    oblo_script_editor
    PRIVATE
    oblo::script::asset
    oblo::asset
    oblo::editor::common
    oblo::nodes::editor
)

add_library(oblo::script::editor ALIAS oblo_script_editor)