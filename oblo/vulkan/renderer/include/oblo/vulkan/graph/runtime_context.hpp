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
        explicit runtime_context(
            render_graph& graph, renderer& renderer, VkCommandBuffer commandBuffer, frame_allocator& frameAllocator) :
            m_graph{&graph},
            m_renderer{&renderer}, m_commandBuffer{commandBuffer}, m_frameAllocator{&frameAllocator}
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

        mesh_table& get_mesh_table() const
        {
            return m_renderer->get_mesh_table();
        }

        frame_allocator& get_frame_allocator() const
        {
            return *m_frameAllocator;
        }

    private:
        render_graph* m_graph;
        renderer* m_renderer;
        VkCommandBuffer m_commandBuffer;
        frame_allocator* m_frameAllocator;
    };
}