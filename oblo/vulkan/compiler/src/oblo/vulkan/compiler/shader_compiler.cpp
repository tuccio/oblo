#include <oblo/vulkan/compiler/shader_compiler.hpp>

#include <oblo/vulkan/compiler/shader_compiler_result.hpp>

namespace oblo::vk
{
    shader_compiler::result::result() = default;

    shader_compiler::result::result(unique_ptr<shader_compiler::result_core> core) : m_core{std::move(core)} {}

    shader_compiler::result::result(result&&) noexcept = default;

    shader_compiler::result& shader_compiler::result::operator=(result&&) noexcept = default;

    shader_compiler::result::~result() = default;

    bool shader_compiler::result::has_errors() const
    {
        return m_core->has_errors();
    }

    string_view shader_compiler::result::get_error_message() const
    {
        return m_core->get_error_message();
    }

    string_view shader_compiler::result::get_source_code() const
    {
        return m_core->get_source_code();
    }

    void shader_compiler::result::get_source_files(deque<string_view>& sourceFiles) const
    {
        return m_core->get_source_files(sourceFiles);
    }

    std::span<const u32> shader_compiler::result::get_spirv() const
    {
        return m_core->get_spirv();
    }
}