oblo_add_library(
    editor
    NAMESPACE oblo::nodes
)

find_package(iconfontcppheaders REQUIRED)

target_link_libraries(
    oblo_nodes_editor
    PUBLIC
    oblo::editor::common
    oblo::nodes::core
    PRIVATE
    iconfontcppheaders::iconfontcppheaders
)