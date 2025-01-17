#include <oblo/vulkan/compiler/compiler_module.hpp>

#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/vulkan/compiler/glsl_compiler.hpp>

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
        glslc_compiler glslc;

        if (glslc.find_glslc())
        {
            constexpr cstring_view workDir = "./glslc";

            filesystem::create_directories(workDir).assert_value();
            glslc.set_work_directory(workDir);
            return std::make_unique<glslc_compiler>(std::move(glslc));
        }

        return std::make_unique<glslang_compiler>();
    }
}