#pragma once

#include <oblo/core/string/string_builder.hpp>
#include <oblo/vulkan/compiler/shader_compiler.hpp>

namespace oblo::vk
{
    class glslang_compiler final : public shader_compiler
    {
    public:
        void init(const shader_compiler_config& config) override;

        result preprocess_from_file(
            allocator& allocator, string_view path, shader_stage stage, string_view preamble) override;

        result compile(result r, const shader_compiler_options& options) override;

    private:
        dynamic_array<string_builder> m_includeDirs;
    };

    class glslc_compiler final : public shader_compiler
    {
    public:
        bool find_glslc();
        void set_work_directory(string_view workDirectory);

        void init(const shader_compiler_config& config) override;

        result preprocess_from_file(
            allocator& allocator, string_view path, shader_stage stage, string_view preamble) override;

        result compile(result r, const shader_compiler_options& options) override;

    private:
        string_builder m_workDirectory;
        string_builder m_glslcPath;
        dynamic_array<string_builder> m_includeDirs;
    };

    cstring_view glsl_deduce_extension(shader_stage stage);
}