oblo_add_library(
    core
    NAMESPACE oblo::gpu
)

target_link_libraries(
    oblo_gpu_core
    PUBLIC
    oblo::core
)