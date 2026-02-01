oblo_add_library(
    vulkan
    NAMESPACE oblo::gpu
)

target_link_libraries(
    oblo_gpu_vulkan
    PUBLIC
    oblo::core
    oblo::gpu::core
)