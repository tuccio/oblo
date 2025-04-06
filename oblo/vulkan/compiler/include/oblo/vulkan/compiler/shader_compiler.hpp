#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/vulkan/compiler/shader_stage.hpp>

#include <span>

namespace oblo::vk
{
    struct shader_preprocessor_options
    {
        /// @brief It can be used to skip emitting line directives, which might confuse SPIR-V tooling in some cases
        bool emitLineDirectives{true};

        /// @brief An optional preamble that will be added at the top of the shader, right after the version directive
        string_view preamble;
    };

    struct shader_compiler_options
    {
        /// @brief Optimize the SPIR-V shader
        bool codeOptimization{false};

        /// @brief Add source-level debug info to the SPIR-V
        bool generateDebugInfo{false};

        /// @brief Path to the source file, allows the SPIR-V output to reference the file correctly when including
        /// debug info.
        cstring_view sourceCodeFilePath{};
    };

    struct shader_compiler_config
    {
        std::span<const string_view> includeDirectories;
    };

    class shader_compiler
    {
    public:
        class result;
        class result_core;

    public:
        virtual ~shader_compiler() = default;

        virtual void init(const shader_compiler_config& config) = 0;

        virtual result preprocess_from_file(allocator& allocator,
            cstring_view path,
            shader_stage stage,
            const shader_preprocessor_options& options) = 0;

        virtual result compile(result r, const shader_compiler_options& options) = 0;
    };

    class shader_compiler::result
    {
    public:
        VK_COMPILER_API result();
        VK_COMPILER_API result(unique_ptr<shader_compiler::result_core> core);
        result(const result&) = delete;
        VK_COMPILER_API result(result&&) noexcept;

        result& operator=(const result&) = delete;
        VK_COMPILER_API result& operator=(result&&) noexcept;

        VK_COMPILER_API ~result();

        VK_COMPILER_API bool has_errors() const;

        VK_COMPILER_API string_view get_error_message() const;

        VK_COMPILER_API string_view get_source_code() const;

        VK_COMPILER_API void get_source_files(deque<string_view>& sourceFiles) const;

        VK_COMPILER_API std::span<const u32> get_spirv() const;

    private:
        unique_ptr<shader_compiler::result_core> m_core;
    };
}