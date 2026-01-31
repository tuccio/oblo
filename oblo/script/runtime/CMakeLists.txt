oblo_add_library(
    runtime
    NAMESPACE oblo::script
)

target_link_libraries(
    oblo_script_runtime
    PUBLIC
    oblo::core
)