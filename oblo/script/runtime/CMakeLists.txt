oblo_add_library(script_rt)

target_link_libraries(
    oblo_script_rt
    PUBLIC
    oblo::core
)

add_library(oblo::script::runtime ALIAS oblo_script_rt)