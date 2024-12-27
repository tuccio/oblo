oblo_add_library(log MODULE)

target_link_libraries(
    oblo_log
    PUBLIC
    oblo::core
    oblo::modules
)