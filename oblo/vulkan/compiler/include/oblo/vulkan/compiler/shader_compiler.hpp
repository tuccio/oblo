#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

#include <memory>
#include <span>

namespace oblo::vk
{
    enum class shader_stage : u8
    {
        mesh,
        task,
        compute,
        vertex,
        geometry,
        tessellation_control,
        tessellation_evaluation,
        fragment,
        raygen,
        intersection,
        closest_hit,
        any_hit,
        miss,
        callable,
    };

    class result;

    struct shader_compiler_options
    {
        bool codeOptimization{false};
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

        virtual result preprocess_from_file(
            allocator& allocator, string_view path, shader_stage stage, string_view preamble) = 0;

        virtual result compile(result r, const shader_compiler_options& options) = 0;
    };

    class shader_compiler::result
    {
    public:
        VK_COMPILER_API result();
        VK_COMPILER_API result(std::unique_ptr<shader_compiler::result_core> core);
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
        std::unique_ptr<shader_compiler::result_core> m_core;
    };
}