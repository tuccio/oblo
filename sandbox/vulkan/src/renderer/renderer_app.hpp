#pragma once

#include <oblo/vulkan/renderer.hpp>
#include <sandbox/context.hpp>

namespace oblo::vk
{
    class renderer_app
    {
    public:
        bool init(const sandbox_init_context& context)
        {
            return m_renderer.init({
                .engine = *context.engine,
                .allocator = *context.allocator,
                .frameAllocator = *context.frameAllocator,
                .resourceManager = *context.resourceManager,
            });
        }

        void shutdown(const sandbox_shutdown_context& context)
        {
            m_renderer.shutdown(*context.frameAllocator);
        }

        void update(const sandbox_render_context& context)
        {
            m_renderer.update({
                .commandBuffer = *context.commandBuffer,
                .frameAllocator = *context.frameAllocator,
                .swapchainTexture = context.swapchainTexture,
            });
        }

        void update_imgui(const sandbox_update_imgui_context&) {}

    private:
        renderer m_renderer;
    };
}