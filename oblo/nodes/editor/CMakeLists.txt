oblo_add_library(nodes_editor)

find_package(iconfontcppheaders REQUIRED)

target_link_libraries(
    oblo_nodes_editor
    PUBLIC
    oblo::editor::common
    oblo::nodes::core
    PRIVATE
    iconfontcppheaders::iconfontcppheaders
)

add_library(oblo::nodes::editor ALIAS oblo_nodes_editor)