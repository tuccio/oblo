oblo_add_library(script_nodes)

target_link_libraries(
    oblo_script_nodes
    PUBLIC
    oblo::core
    PRIVATE
    oblo::script::runtime
)

add_library(oblo::script::nodes ALIAS oblo_script_nodes)