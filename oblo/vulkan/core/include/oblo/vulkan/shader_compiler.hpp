#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>

#include <span>
#include <vector>

#include <vulkan/vulkan.h>

namespace oblo
{
    class frame_allocator;
    class string_builder;
}

namespace oblo::vk::shader_compiler
{
    class include_handler;
    class scope;

    struct options
    {
        include_handler* includeHandler{nullptr};
        bool codeOptimization{false};
        bool generateDebugInfo{false};
    };

    void init();
    void shutdown();

    bool compile_glsl_to_spirv(string_view debugName,
        string_view sourceCode,
        VkShaderStageFlagBits stage,
        std::vector<unsigned>& outSpirv,
        const options& options = {});

    VkShaderModule create_shader_module_from_spirv(
        VkDevice device, std::span<const unsigned> spirv, const VkAllocationCallbacks* allocationCbs);

    VkShaderModule create_shader_module_from_glsl_file(frame_allocator& allocator,
        VkDevice device,
        VkShaderStageFlagBits stage,
        cstring_view filePath,
        const VkAllocationCallbacks* allocationCbs,
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

        virtual bool resolve(string_view header, string_builder& path) = 0;
    };
}