oblo_add_library(script_rt)

oblo_register_core_module(script_rt)

target_link_libraries(
    oblo_script_rt
    PUBLIC
    oblo::modules
    PRIVATE
    oblo::log
)

add_library(oblo::script::runtime ALIAS oblo_script_rt)