#pragma once

#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/texture.hpp>

#include <string_view>

namespace oblo::vk
{
    struct buffer;
    struct texture;
}

namespace oblo::vk
{
    class render_graph;
    class pass_manager;

    class runtime_context
    {
    public:
        explicit runtime_context(render_graph& graph, renderer& renderer, VkCommandBuffer commandBuffer) :
            m_graph{&graph}, m_renderer{&renderer}, m_commandBuffer{commandBuffer}
        {
        }

        texture access(resource<texture> h) const;

        buffer access(resource<buffer> h) const;

        template <typename T>
        T* access(data<T> h) const
        {
            return static_cast<T*>(access_resource_storage(h.value));
        }

        VkCommandBuffer get_command_buffer() const
        {
            return m_commandBuffer;
        }

        pass_manager& get_pass_manager() const
        {
            return m_renderer->get_pass_manager();
        }

        resource_manager& get_resource_manager() const
        {
            return m_renderer->get_resource_manager();
        }

        draw_registry& get_draw_registry() const
        {
            return m_renderer->get_draw_registry();
        }

        string_interner& get_string_interner() const
        {
            return m_renderer->get_string_interner();
        }

    private:
        void* access_resource_storage(u32 index) const;

    private:
        render_graph* m_graph;
        renderer* m_renderer;
        VkCommandBuffer m_commandBuffer;
    };
}