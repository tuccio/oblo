oblo_add_library(dotnet_editor MODULE)

oblo_register_editor_module(dotnet_editor)

target_link_libraries(
    oblo_dotnet_editor
    PRIVATE
    oblo::asset
    oblo::dotnet::asset
    oblo::editor::core
    oblo::editor::common
)

add_library(oblo::dotnet::editor ALIAS oblo_dotnet_editor)
