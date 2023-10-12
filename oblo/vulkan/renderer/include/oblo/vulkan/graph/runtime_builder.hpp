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

    class runtime_builder
    {
    public:
        explicit runtime_builder(render_graph& graph) : m_graph{&graph} {}

        void create(resource<texture> texture, const texture2d_initializer& initializer, resource_usage usage)
        {
            // TODO
            (void) texture;
            (void) initializer;
            (void) usage;
        }

        void read(resource<texture> texture, resource_usage usage)
        {
            // TODO
            (void) texture;
            (void) usage;
        }

        void write(resource<texture> texture, resource_usage usage)
        {
            // TODO
            (void) texture;
            (void) usage;
        }

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(m_graph->access_data(data.value));
        }

    private:
        render_graph* m_graph;
    };
}