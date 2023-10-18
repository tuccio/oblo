#pragma once

#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    struct buffer;
    struct texture;
}

namespace oblo::vk
{
    class render_graph;

    class runtime_context
    {
    public:
        explicit runtime_context(render_graph& graph,
                                 resource_manager& resourceManager,
                                 VkCommandBuffer commandBuffer) :
            m_graph{&graph},
            m_resourceManager{&resourceManager}, m_commandBuffer{commandBuffer}
        {
        }

        texture access(resource<texture> h) const;

        void* access(data<texture> h) const;

        template <typename T>
        T* access(data<T> h) const
        {
            return static_cast<T*>(access_data(h.value));
        }

        VkCommandBuffer get_command_buffer() const
        {
            return m_commandBuffer;
        }

    private:
        void* access_data(u32 h) const;

    private:
        render_graph* m_graph;
        resource_manager* m_resourceManager;
        VkCommandBuffer m_commandBuffer;
    };
}