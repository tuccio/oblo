#pragma once

#include <oblo/modules/module_interface.hpp>

#include <memory>

namespace oblo::vk
{
    class shader_compiler;

    class compiler_module final : public module_interface
    {
    public:
        VK_COMPILER_API bool startup(const module_initializer& initializer) override;
        VK_COMPILER_API void shutdown() override;
        VK_COMPILER_API void finalize() override;

        VK_COMPILER_API std::unique_ptr<shader_compiler> make_glsl_compiler() const;
    };
}