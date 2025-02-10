#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::vk
{
    class shader_compiler;

    class compiler_module final : public module_interface
    {
    public:
        VK_COMPILER_API bool startup(const module_initializer& initializer) override;
        VK_COMPILER_API void shutdown() override;
        VK_COMPILER_API bool finalize() override;

        VK_COMPILER_API unique_ptr<shader_compiler> make_glslc_compiler(cstring_view workDir) const;
        VK_COMPILER_API unique_ptr<shader_compiler> make_glslang_compiler() const;
    };
}