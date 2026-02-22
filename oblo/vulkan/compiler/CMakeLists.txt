oblo_add_library(
    compiler MODULE
    NAMESPACE oblo::vulkan
)

find_package(glslang REQUIRED)

target_link_libraries(
    oblo_vulkan_compiler
    PUBLIC
    oblo::core
    oblo::gpu::core
    oblo::modules
    PRIVATE
    oblo::log
    glslang::glslang
    glslang::SPIRV
)