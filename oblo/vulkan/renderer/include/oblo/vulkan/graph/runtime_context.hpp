#pragma once

#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    struct buffer;
    struct texture;
}

namespace oblo::vk
{
    class render_graph;
    class render_pass_manager;

    class runtime_context
    {
    public:
        explicit runtime_context(render_graph& graph, renderer& renderer, VkCommandBuffer commandBuffer) :
            m_graph{&graph}, m_renderer{&renderer}, m_commandBuffer{commandBuffer}
        {
        }

        texture access(resource<texture> h) const;

        template <typename T>
        T* access(data<T> h) const
        {
            return static_cast<T*>(m_graph->access_resource_storage(h.value));
        }

        VkCommandBuffer get_command_buffer() const
        {
            return m_commandBuffer;
        }

        render_pass_manager& get_render_pass_manager() const
        {
            return m_renderer->get_render_pass_manager();
        }

        resource_manager& get_resource_manager() const
        {
            return m_renderer->get_resource_manager();
        }

        draw_registry& get_draw_registry() const
        {
            return m_renderer->get_draw_registry();
        }

    private:
        render_graph* m_graph;
        renderer* m_renderer;
        VkCommandBuffer m_commandBuffer;
    };
}