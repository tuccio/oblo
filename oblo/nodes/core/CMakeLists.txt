oblo_add_library(
    core
    NAMESPACE oblo::nodes
)

target_link_libraries(
    oblo_nodes_core
    PUBLIC
    oblo::ast
    oblo::core
    oblo::properties
)