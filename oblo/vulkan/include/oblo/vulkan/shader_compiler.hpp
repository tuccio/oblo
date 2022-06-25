#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class shader_compiler
    {
    public:
        shader_compiler();
        shader_compiler(const shader_compiler&) = delete;
        shader_compiler(shader_compiler&&) noexcept = default;
        shader_compiler& operator=(const shader_compiler&) = delete;
        shader_compiler& operator=(shader_compiler&&) noexcept = default;
        ~shader_compiler();

        VkShaderModule create_shader_module_from_glsl_file(VkDevice device,
                                                           const std::filesystem::path& sourceFile,
                                                           VkShaderStageFlagBits stage);

        VkShaderModule create_shader_module_from_glsl_source(VkDevice device,
                                                             std::string_view sourceCode,
                                                             VkShaderStageFlagBits stage);

    private:
        std::string m_codeBuffer;
        std::vector<unsigned> m_spirvBuffer;
    };
}