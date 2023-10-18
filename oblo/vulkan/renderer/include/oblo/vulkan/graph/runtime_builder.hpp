#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    struct buffer;
    struct texture;

    enum class resource_usage
    {
        render_target_write,
        depth_buffer_write,
        shader_read,
    };

    struct texture2d_initializer
    {
        u32 width;
        u32 height;
        VkFormat format;
        VkImageUsageFlags usage;
        VkImageAspectFlags aspectMask;
    };

    class render_graph;
    class resource_pool;

    class runtime_builder
    {
    public:
        explicit runtime_builder(render_graph& graph, resource_pool& resourcePool) :
            m_graph{&graph}, m_resourcePool{&resourcePool}
        {
        }

        void create(resource<texture> texture, const texture2d_initializer& initializer, resource_usage usage);

        void use(resource<texture> texture, resource_usage usage);

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(m_graph->access_data(data.value));
        }

    private:
        render_graph* m_graph;
        resource_pool* m_resourcePool;
    };
}