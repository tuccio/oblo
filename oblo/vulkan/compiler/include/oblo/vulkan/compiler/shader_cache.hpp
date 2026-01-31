#pragma once

#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/compiler/shader_compiler.hpp>

#include <span>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class shader_cache
    {
    public:
        OBLO_VK_COMPILER_API bool init(string_view dir);

        OBLO_VK_COMPILER_API void set_glsl_compiler(shader_compiler* glslCompiler);
        OBLO_VK_COMPILER_API shader_compiler* get_glsl_compiler() const;

        OBLO_VK_COMPILER_API void set_cache_enabled(bool enable);

        OBLO_VK_COMPILER_API shader_compiler::result find_or_compile(frame_allocator& allocator,
            cstring_view filePath,
            shader_stage stage,
            const shader_preprocessor_options& preprocessorOptions,
            const shader_compiler_options& compilerOptions,
            string_view debugName);

    private:
        string_builder m_path;
        shader_compiler* m_glslCompiler{};
        bool m_cacheEnabled{};
    };
}