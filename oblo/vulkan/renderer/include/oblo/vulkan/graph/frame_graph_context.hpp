#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/frame_graph_resources.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <vulkan/vulkan_core.h>

#include <span>

namespace oblo
{
    class frame_allocator;
    class random_generator;
    class string_interner;

    class string;
}

namespace oblo::vk
{
    class draw_registry;
    class pass_manager;
    class renderer;
    class resource_manager;
    class resource_pool;

    struct bindable_object;
    struct buffer;
    struct texture;
    struct image_initializer;

    struct loaded_functions;
    struct frame_graph_impl;
    struct frame_graph_pin_storage;
    struct resident_texture;
    struct staging_buffer_span;

    using binding_table = flat_dense_map<h32<string>, bindable_object>;

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
        storage_read,
        storage_write,
        transfer_source,
        transfer_destination,
    };

    enum class buffer_usage : u8
    {
        storage_read,
        storage_write,
        /// @brief This means the buffer is not actually used on GPU in this node, just uploaded on.
        storage_upload,
        uniform,
        indirect,
        enum_max,
    };

    enum class pass_kind : u8
    {
        none,
        graphics,
        compute,
        raytracing,
        transfer,
    };

    class frame_graph_build_context
    {
    public:
        explicit frame_graph_build_context(
            frame_graph_impl& frameGraph, renderer& renderer, resource_pool& resourcePool);

        void create(
            resource<texture> texture, const texture_resource_initializer& initializer, texture_usage usage) const;

        void create(resource<buffer> buffer,
            const buffer_resource_initializer& initializer,
            pass_kind passKind,
            buffer_usage usage) const;

        void create(resource<buffer> buffer,
            const staging_buffer_span& stagedData,
            pass_kind passKind,
            buffer_usage usage) const;

        void acquire(resource<texture> texture, texture_usage usage) const;

        h32<resident_texture> acquire_bindless(resource<texture> texture, texture_usage usage) const;

        void acquire(resource<buffer> buffer, pass_kind passKind, buffer_usage usage) const;

        [[nodiscard]] resource<buffer> create_dynamic_buffer(
            const buffer_resource_initializer& initializer, pass_kind passKind, buffer_usage usage) const;

        [[nodiscard]] resource<buffer> create_dynamic_buffer(
            const staging_buffer_span& stagedData, pass_kind passKind, buffer_usage usage) const;

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        std::span<const T> access(data_sink<T> data) const
        {
            return *static_cast<data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        void push(data_sink<T> data, T&& value) const
        {
            auto* a = static_cast<data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
            a->push_back(std::move(value));
        }

        expected<image_initializer> get_current_initializer(resource<texture> texture) const;

        frame_allocator& get_frame_allocator() const;

        const draw_registry& get_draw_registry() const;

        random_generator& get_random_generator() const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
        resource_pool& m_resourcePool;
    };

    struct buffer_binding_desc
    {
        string_view name;
        resource<buffer> resource;
    };

    struct texture_binding_desc
    {
        string_view name;
        resource<texture> resource;
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

        /// @brief Queries the number of frames a stable texture has been alive for.
        /// On the first frame of usage the function will return 0.
        /// For transient textures it will always return 0.
        /// @param texture A valid texture resource.
        u32 get_frames_alive_count(resource<texture> texture) const;

        void upload(resource<buffer> h, std::span<const byte> data, u32 bufferOffset = 0) const;

        VkCommandBuffer get_command_buffer() const;

        VkDevice get_device() const;

        pass_manager& get_pass_manager() const;

        draw_registry& get_draw_registry() const;

        string_interner& get_string_interner() const;

        const loaded_functions& get_loaded_functions() const;

        void bind_buffers(binding_table& table, std::initializer_list<buffer_binding_desc> bindings) const;
        void bind_textures(binding_table& table, std::initializer_list<texture_binding_desc> bindings) const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
        VkCommandBuffer m_commandBuffer;
    };
}