oblo_add_executable(
    gpu_sandbox
    NAMESPACE oblo::gpu
)

target_link_libraries(
    gpu_sandbox
    PRIVATE
    oblo::app
    oblo::gpu::core
    oblo::gpu::vulkan
)