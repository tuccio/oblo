oblo_add_library(nodes_core)

target_link_libraries(
    oblo_nodes_core
    PUBLIC
    oblo::core
    oblo::properties
)

add_library(oblo::nodes::core ALIAS oblo_nodes_core)