oblo_add_library(
    editor MODULE
    NAMESPACE oblo::script
)

oblo_register_editor_module(oblo::script::editor)

find_package(iconfontcppheaders REQUIRED)

target_link_libraries(
    oblo_script_editor
    PRIVATE
    oblo::script::asset
    oblo::asset
    oblo::editor::common
    oblo::nodes::editor
    iconfontcppheaders::iconfontcppheaders
)