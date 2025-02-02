oblo_add_library(vk_compiler MODULE)

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