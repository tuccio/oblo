#pragma once

#include <oblo/vulkan/allocator.hpp>

namespace oblo::vk
{
    struct sandbox_init_context;
    struct sandbox_shutdown_context;
    struct sandbox_render_context;

    class helloworld
    {
    public:
        bool init(const sandbox_init_context& context);
        void shutdown(const sandbox_shutdown_context& context);
        void update(const sandbox_render_context& context);
        void update_imgui();

    private:
        bool create_shader_modules(VkDevice device);
        bool create_graphics_pipeline(VkDevice device, const VkFormat swapchainFormat);
        bool create_vertex_buffers(allocator& allocator);

        void destroy_graphics_pipeline(VkDevice device);

    private:
        VkShaderModule m_vertShaderModule{nullptr};
        VkShaderModule m_fragShaderModule{nullptr};
        VkPipelineLayout m_pipelineLayout{nullptr};
        VkPipeline m_graphicsPipeline{nullptr};

        allocator::buffer m_positions{};
        allocator::buffer m_colors{};
    };
}