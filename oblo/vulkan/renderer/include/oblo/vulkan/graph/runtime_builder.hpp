#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <vulkan/vulkan.h>

#include <span>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class draw_registry;
    class renderer;

    struct buffer;
    struct texture;

    enum class resource_usage
    {
        render_target_write,
        depth_stencil_read,
        depth_stencil_write,
        shader_read,
        transfer_source,
        transfer_destination,
    };

    struct transient_texture_initializer
    {
        u32 width;
        u32 height;
        VkFormat format;
        VkImageUsageFlags usage;
        VkImageAspectFlags aspectMask;
    };

    struct transient_buffer_initializer
    {
        u32 size;
        std::span<const std::byte> data;
    };

    class render_graph;
    class resource_pool;

    class runtime_builder
    {
    public:
        explicit runtime_builder(render_graph& graph, resource_pool& resourcePool, renderer& renderer) :
            m_graph{&graph}, m_resourcePool{&resourcePool}, m_renderer{&renderer}
        {
        }

        void create(
            resource<texture> texture, const transient_texture_initializer& initializer, resource_usage usage) const;

        void create(resource<buffer> buffer, const transient_buffer_initializer& initializer) const;

        void acquire(resource<texture> texture, resource_usage usage) const;

        resource<buffer> create_dynamic_buffer(const transient_buffer_initializer& initializer) const;

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_resource_storage(data.value));
        }

        frame_allocator& get_frame_allocator() const;

        const draw_registry& get_draw_registry() const;

    private:
        void* access_resource_storage(u32 index) const;

    private:
        render_graph* m_graph;
        resource_pool* m_resourcePool;
        renderer* m_renderer;
    };
}