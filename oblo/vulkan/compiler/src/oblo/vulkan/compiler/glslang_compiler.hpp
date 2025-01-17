#pragma once

#include <oblo/core/string/string_builder.hpp>
#include <oblo/vulkan/compiler/shader_compiler.hpp>

namespace oblo::vk
{
    class glslang_compiler final : public shader_compiler
    {
    public:
        void set_search_directories(std::span<const string_view> paths) override;

        result preprocess_from_file(
            allocator& allocator, string_view path, shader_stage stage, string_view preamble) override;

        result compile(result r, const shader_compiler_options& options) override;

    private:
        dynamic_array<string_builder> m_includeDirs;
    };
}