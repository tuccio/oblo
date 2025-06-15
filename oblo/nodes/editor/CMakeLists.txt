oblo_add_library(nodes_editor)

target_link_libraries(
    oblo_nodes_editor
    PUBLIC
    oblo::editor::common
    oblo::nodes::core
)

add_library(oblo::nodes::editor ALIAS oblo_nodes_editor)