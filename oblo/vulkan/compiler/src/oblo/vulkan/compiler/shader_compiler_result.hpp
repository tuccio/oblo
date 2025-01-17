#pragma once

#include <oblo/vulkan/compiler/shader_compiler.hpp>

namespace oblo::vk
{
    class shader_compiler::result_core
    {
    public:
        virtual ~result_core() = default;

        virtual bool has_errors() = 0;

        virtual string_view get_error_message() = 0;

        virtual string_view get_source_code() = 0;

        virtual void get_source_files(deque<string_view>& sourceFiles) = 0;

        virtual std::span<const u32> get_spirv() = 0;
    };

    inline shader_compiler::result_core* get_shader_compiler_result_core(shader_compiler::result& r);

    template <typename T>
    T& get_shader_compiler_result_core_as(shader_compiler::result& r);

    template <auto F>
    struct shader_compiler_result_core_access
    {
        friend inline shader_compiler::result_core* get_shader_compiler_result_core(shader_compiler::result& r)
        {
            return (r.*F).get();
        }
    };

    template struct shader_compiler_result_core_access<&shader_compiler::result::m_core>;

    template <typename T>
    T& get_shader_compiler_result_core_as(shader_compiler::result& r)
    {
        shader_compiler::result_core* const core = get_shader_compiler_result_core(r);
        return *dynamic_cast<T*>(core);
    }
}