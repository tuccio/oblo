oblo_add_library(editor MODULE
    NAMESPACE oblo::dotnet
)

oblo_register_editor_module(oblo::dotnet::editor)

target_link_libraries(
    oblo_dotnet_editor
    PRIVATE
    oblo::asset
    oblo::dotnet::asset
    oblo::editor::core
    oblo::editor::common
)
