oblo_add_library(nodes_common)

target_link_libraries(
    oblo_nodes_common
    PUBLIC
    oblo::nodes::core
)

add_library(oblo::nodes::common ALIAS oblo_nodes_common)