oblo_add_library(
    common
    NAMESPACE oblo::nodes
)

target_link_libraries(
    oblo_nodes_common
    PUBLIC
    oblo::nodes::core
)