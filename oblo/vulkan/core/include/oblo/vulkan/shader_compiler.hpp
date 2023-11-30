#pragma once

#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk::shader_compiler
{
    class include_handler;
    class scope;

    struct options
    {
        include_handler* includeHandler{nullptr};
        bool codeOptimization{false};
    };

    void init();
    void shutdown();

    bool compile_glsl_to_spirv(std::string_view debugName,
        std::string_view sourceCode,
        VkShaderStageFlagBits stage,
        std::vector<unsigned>& outSpirv,
        const options& options = {});

    VkShaderModule create_shader_module_from_spirv(VkDevice device, std::span<const unsigned> spirv);

    VkShaderModule create_shader_module_from_glsl_file(frame_allocator& allocator,
        VkDevice device,
        VkShaderStageFlagBits stage,
        std::string_view filePath,
        const options& options = {});

    class scope
    {
    public:
        scope()
        {
            init();
        }

        scope(const scope&) = delete;
        scope(scope&&) noexcept = delete;

        scope& operator=(const scope&) = delete;
        scope& operator=(scope&&) noexcept = delete;

        ~scope()
        {
            shutdown();
        }
    };

    class include_handler
    {
    public:
        virtual ~include_handler() = default;

        virtual frame_allocator& get_allocator() = 0;

        virtual bool resolve(std::string_view header, std::filesystem::path& path) = 0;
    };
}