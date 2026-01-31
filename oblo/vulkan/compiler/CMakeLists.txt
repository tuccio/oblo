oblo_add_library(
    compiler MODULE
    NAMESPACE oblo::vk
)

find_package(glslang REQUIRED)

target_link_libraries(
    oblo_vk_compiler
    PUBLIC
    oblo::core
    oblo::modules
    PRIVATE
    oblo::log
    glslang::glslang
    glslang::SPIRV
)