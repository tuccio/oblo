#include <oblo/vulkan/compiler/compiler_module.hpp>

#include <oblo/vulkan/compiler/glslang_compiler.hpp>

#include <glslang/Public/ShaderLang.h>

namespace oblo::vk
{
    bool compiler_module::startup(const module_initializer&)
    {
        return glslang::InitializeProcess();
    }

    void compiler_module::shutdown()
    {
        glslang::FinalizeProcess();
    }

    void compiler_module::finalize() {}

    std::unique_ptr<shader_compiler> compiler_module::make_glsl_compiler() const
    {
        return std::make_unique<glslang_compiler>();
    }
}