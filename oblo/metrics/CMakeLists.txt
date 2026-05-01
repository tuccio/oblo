oblo_add_library(metrics SHARED)

target_link_libraries(
    oblo_metrics
    PUBLIC
    oblo::core
    oblo::modules
)