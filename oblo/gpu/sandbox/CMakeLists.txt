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
    oblo::vulkan::compiler
)

oblo_add_data_folder(oblo::gpu::sandbox_shaders "${CMAKE_CURRENT_SOURCE_DIR}/resources")

oblo_symlink_to_data_folder(oblo::gpu_sandbox oblo::gpu::sandbox_shaders "$<TARGET_FILE_DIR:oblo::gpu_sandbox>/gpu_sandbox")
