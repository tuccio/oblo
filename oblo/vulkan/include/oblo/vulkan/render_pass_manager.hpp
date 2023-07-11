#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/shader_compiler.hpp>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    struct render_pass;
    struct render_pass_initializer;
    struct render_pipeline;
    struct render_pipeline_initializer;

    class render_pass_manager
    {
    public:
        render_pass_manager();
        render_pass_manager(const render_pass_manager&) = delete;
        render_pass_manager(render_pass_manager&&) noexcept = delete;
        render_pass_manager& operator=(const render_pass_manager&) = delete;
        render_pass_manager& operator=(render_pass_manager&&) noexcept = delete;
        ~render_pass_manager();

        void init(VkDevice device);
        void shutdown();

        handle<render_pass> register_render_pass(const render_pass_initializer& desc);

        handle<render_pipeline> get_or_create_pipeline(frame_allocator& allocator,
                                                       handle<render_pass> handle,
                                                       const render_pipeline_initializer& desc);

        void bind(VkCommandBuffer commandBuffer, handle<render_pipeline> handle);

    private:
        VkDevice m_device{};
        u32 m_lastRenderPassId{};
        u32 m_lastRenderPipelineId{};
        flat_dense_map<handle<render_pass>, render_pass> m_renderPasses;
        flat_dense_map<handle<render_pipeline>, render_pipeline> m_renderPipelines;
    };
}
