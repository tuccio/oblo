oblo_add_library(script_compiler)

target_link_libraries(
    oblo_script_compiler
    PUBLIC
    oblo::ast
    oblo::script::runtime
)

add_library(oblo::script::compiler ALIAS oblo_script_compiler)