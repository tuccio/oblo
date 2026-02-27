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
        OBLO_VULKAN_COMPILER_API bool startup(const module_initializer& initializer) override;
        OBLO_VULKAN_COMPILER_API void shutdown() override;
        OBLO_VULKAN_COMPILER_API bool finalize() override;

        OBLO_VULKAN_COMPILER_API unique_ptr<shader_compiler> make_glslc_compiler(cstring_view workDir) const;
        OBLO_VULKAN_COMPILER_API unique_ptr<shader_compiler> make_glslang_compiler() const;
    };
}