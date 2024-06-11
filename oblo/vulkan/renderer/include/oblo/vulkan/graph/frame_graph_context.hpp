#pragma once

#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <vulkan/vulkan_core.h>

#include <span>
#include <string_view>

namespace oblo
{
    class frame_allocator;
    class string_interner;

    struct string;
}

namespace oblo::vk
{
    class draw_registry;
    class pass_manager;
    class renderer;
    class resource_manager;
    class resource_pool;

    struct buffer;
    struct texture;

    struct frame_graph_impl;
    struct frame_graph_pin_storage;
    struct staging_buffer_span;

    using buffer_binding_table = flat_dense_map<h32<string>, buffer>;

    class frame_graph_init_context
    {
    public:
        explicit frame_graph_init_context(renderer& renderer);

        pass_manager& get_pass_manager() const;

        string_interner& get_string_interner() const;

    private:
        renderer& m_renderer;
    };

    enum class texture_usage : u8
    {
        render_target_write,
        depth_stencil_read,
        depth_stencil_write,
        shader_read,
        transfer_source,
        transfer_destination,
    };

    enum class buffer_usage : u8
    {
        storage_read,
        storage_write,
        uniform,
        indirect,
        enum_max,
    };

    enum class pass_kind : u8
    {
        graphics,
        compute,
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
        std::span<const byte> data;
    };

    class frame_graph_build_context
    {
    public:
        explicit frame_graph_build_context(
            frame_graph_impl& frameGraph, renderer& renderer, resource_pool& resourcePool);

        void create(
            resource<texture> texture, const transient_texture_initializer& initializer, texture_usage usage) const;

        void create(resource<buffer> buffer,
            const transient_buffer_initializer& initializer,
            pass_kind passKind,
            buffer_usage usage) const;

        void create(resource<buffer> buffer,
            const staging_buffer_span& stagedData,
            pass_kind passKind,
            buffer_usage usage) const;

        void acquire(resource<texture> texture, texture_usage usage) const;

        void acquire(resource<buffer> buffer, pass_kind passKind, buffer_usage usage) const;

        resource<buffer> create_dynamic_buffer(
            const transient_buffer_initializer& initializer, pass_kind passKind, buffer_usage usage) const;

        resource<buffer> create_dynamic_buffer(
            const staging_buffer_span& stagedData, pass_kind passKind, buffer_usage usage) const;

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        frame_allocator& get_frame_allocator() const;

        const draw_registry& get_draw_registry() const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
        resource_pool& m_resourcePool;
    };

    struct pin_binding_desc
    {
        resource<buffer> buffer;
        std::string_view name;
    };

    class frame_graph_execute_context
    {
    public:
        explicit frame_graph_execute_context(
            frame_graph_impl& frameGraph, renderer& renderer, VkCommandBuffer commandBuffer);

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        texture access(resource<texture> h) const;

        buffer access(resource<buffer> h) const;

        VkCommandBuffer get_command_buffer() const;

        pass_manager& get_pass_manager() const;

        resource_manager& get_resource_manager() const;

        draw_registry& get_draw_registry() const;

        string_interner& get_string_interner() const;

        void add_bindings(buffer_binding_table& table, std::initializer_list<pin_binding_desc> bindings) const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
        VkCommandBuffer m_commandBuffer;
    };
}