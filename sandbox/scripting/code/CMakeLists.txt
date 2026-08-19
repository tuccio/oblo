oblo_add_library(scripting_sandbox MODULE)

target_link_libraries(
    oblo_scripting_sandbox
    PRIVATE
    oblo::log
    oblo::modules
)