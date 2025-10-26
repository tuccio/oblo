oblo_add_library(metrics)

target_link_libraries(
    oblo_metrics
    PUBLIC
    oblo::core
)