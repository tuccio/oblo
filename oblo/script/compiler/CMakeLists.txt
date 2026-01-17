oblo_add_library(script_compiler)

target_link_libraries(
    oblo_script_compiler
    PUBLIC
    oblo::ast
    oblo::script::runtime
)

add_library(oblo::script::compiler ALIAS oblo_script_compiler)

# We have includes for our native library implementation in the resources   
target_include_directories(oblo_test_script_compiler PRIVATE resources/cppgenlib)