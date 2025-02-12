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

    bool compiler_module::finalize()
    {
        return true;
    }

    unique_ptr<shader_compiler> compiler_module::make_glslc_compiler(cstring_view workDir) const
    {
        glslc_compiler glslc;

        if (glslc.find_glslc())
        {
            filesystem::create_directories(workDir).assert_value();
            glslc.set_work_directory(workDir);
            return allocate_unique<glslc_compiler>(std::move(glslc));
        }

        return nullptr;
    }

    unique_ptr<shader_compiler> compiler_module::make_glslang_compiler() const
    {
        return allocate_unique<glslang_compiler>();
    }
}