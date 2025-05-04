#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/frame_graph_resources.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    class async_download;
    class frame_allocator;
    class random_generator;
    class texture;

    template <typename>
    class resource_ptr;

    template <typename E, u32 Size>
    struct flags;

    struct vec2u;
}

namespace oblo::vk
{
    class renderer;

    struct gpu_info;
    struct texture_init_desc;

    class binding_tables_span;

    class frame_graph_init_context
    {
    public:
        explicit frame_graph_init_context(frame_graph_impl& frameGraph, renderer& renderer);

        h32<compute_pass> register_compute_pass(const compute_pass_initializer& initializer) const;
        h32<render_pass> register_render_pass(const render_pass_initializer& initializer) const;
        h32<raytracing_pass> register_raytracing_pass(const raytracing_pass_initializer& initializer) const;

        const gpu_info& get_gpu_info() const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
    };

    class frame_graph_build_context
    {
    public:
        explicit frame_graph_build_context(
            frame_graph_impl& frameGraph, frame_graph_build_state& state, renderer& renderer);

        [[nodiscard]] h32<compute_pass_instance> compute_pass(h32<compute_pass> pass,
            const compute_pipeline_initializer& initializer) const;

        [[nodiscard]] h32<render_pass_instance> render_pass(h32<render_pass> pass,
            const render_pipeline_initializer& initializer) const;

        [[nodiscard]] h32<raytracing_pass_instance> raytracing_pass(h32<raytracing_pass> pass,
            const raytracing_pipeline_initializer& initializer) const;

        [[nodiscard]] h32<transfer_pass_instance> transfer_pass() const;

        h32<empty_pass_instance> empty_pass() const;

        void create(
            resource<texture> texture, const texture_resource_initializer& initializer, texture_usage usage) const;

        void create(resource<buffer> buffer, const buffer_resource_initializer& initializer, buffer_usage usage) const;

        void create(resource<buffer> buffer, const staging_buffer_span& stagedData, buffer_usage usage) const;

        void register_texture(resource<texture> resource, h32<texture> externalTexture) const;

        // Temporary solution until the acceleration structure is a proper resource.
        void register_global_tlas(VkAccelerationStructureKHR accelerationStructure) const;

        void acquire(resource<texture> texture, texture_usage usage) const;

        h32<resident_texture> acquire_bindless(resource<texture> texture, texture_usage usage) const;

        h32<resident_texture> load_resource(const resource_ptr<oblo::texture>& texture) const;

        void acquire(resource<buffer> buffer, buffer_usage usage) const;

        void reroute(resource<buffer> source, resource<buffer> destination) const;
        void reroute(resource<texture> source, resource<texture> destination) const;

        /// @brief Determines whether the pin contributes to a frame graph output.
        /// @remarks Nodes might decide to skip some computation if certain nodes don't contribute to any frame graph
        /// output.
        bool is_active_output(resource<texture> texture) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(resource<buffer> buffer) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(resource<texture> texture) const;

        [[nodiscard]] resource<buffer> create_dynamic_buffer(const buffer_resource_initializer& initializer,
            buffer_usage usage) const;

        [[nodiscard]] resource<buffer> create_dynamic_buffer(const staging_buffer_span& stagedData,
            buffer_usage usage) const;

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

        template <typename T>
        void push(data_sink<T> data, const T& value) const
        {
            auto* a = static_cast<data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
            a->push_back(value);
        }

        expected<texture_init_desc> get_current_initializer(resource<texture> texture) const;

        frame_allocator& get_frame_allocator() const;

        random_generator& get_random_generator() const;

        staging_buffer_span stage_upload(std::span<const byte> data) const;

        u32 get_current_frames_count() const;

        template <typename T>
        bool has_event() const
        {
            return has_event_impl(get_type_id<T>());
        }

        const gpu_info& get_gpu_info() const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

        bool has_event_impl(const type_id& type) const;

    private:
        frame_graph_impl& m_frameGraph;
        frame_graph_build_state& m_state;
        renderer& m_renderer;
    };

    class frame_graph_execute_context
    {
    public:
        explicit frame_graph_execute_context(
            const frame_graph_impl& frameGraph, frame_graph_execution_state& executeCtx, renderer& renderer);

        void begin_pass(h32<frame_graph_pass> handle) const;

        expected<> begin_pass(h32<compute_pass_instance> handle) const;

        expected<> begin_pass(h32<render_pass_instance> handle, const render_pass_config& cfg) const;

        expected<> begin_pass(h32<raytracing_pass_instance> handle) const;

        expected<> begin_pass(h32<transfer_pass_instance> handle) const;

        expected<> begin_pass(h32<empty_pass_instance> handle) const;

        void end_pass() const;

        void bind_descriptor_sets(binding_tables_span bindingTables) const;

        void bind_index_buffer(resource<buffer> buffer, u32 bufferOffset, mesh_index_type indexType) const;

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

        resource<acceleration_structure> get_global_tlas() const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(resource<buffer> buffer) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(resource<texture> texture) const;

        /// @brief Queries the number of frames a stable texture has been alive for.
        /// On the first frame of usage the function will return 0.
        /// For transient textures it will always return 0.
        /// @param texture A valid texture resource.
        u32 get_frames_alive_count(resource<texture> texture) const;

        /// @brief Queries the number of frames a stable buffer has been alive for.
        /// On the first frame of usage the function will return 0.
        /// For transient buffers it will always return 0.
        /// @param buffer A valid buffer resource.
        u32 get_frames_alive_count(resource<buffer> buffer) const;

        u32 get_current_frames_count() const;

        // TODO: This should probably be deprecated, it would be hard to make this thread-safe, staging should happen
        // when building instead.
        void upload(resource<buffer> h, std::span<const byte> data, u32 bufferOffset = 0) const;

        void upload(resource<buffer> h, const staging_buffer_span& data, u32 bufferOffset = 0) const;

        async_download download(resource<buffer> h) const;

        u64 get_device_address(resource<buffer> buffer) const;

        template <typename T>
        bool has_event() const
        {
            return has_event_impl(get_type_id<T>());
        }

        const gpu_info& get_gpu_info() const;

        void set_viewport(u32 w, u32 h, f32 minDepth = 0.f, f32 maxDepth = 1.f) const;
        void set_scissor(i32 x, i32 y, u32 w, u32 h) const;

        void push_constants(flags<shader_stage, 14> stages, u32 offset, std::span<const byte> bytes) const;
        void dispatch_compute(u32 groupsX, u32 groupsY, u32 groupsZ) const;
        void trace_rays(u32 x, u32 y, u32 z) const;

        void draw_indexed(u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const;

        void draw_mesh_tasks_indirect_count(resource<buffer> drawCallBuffer,
            u32 drawCallBufferOffset,
            resource<buffer> drawCallCountBuffer,
            u32 drawCallCountBufferOffset,
            u32 maxDrawCount) const;

        void blit_color(resource<texture> srcTexture, resource<texture> dstTexture) const;

        vec2u get_resolution(resource<texture> h) const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

        buffer access(resource<buffer> h) const;
        texture access(resource<texture> h) const;

        bool has_event_impl(const type_id& type) const;

    private:
        const frame_graph_impl& m_frameGraph;
        frame_graph_execution_state& m_state;
        renderer& m_renderer;
    };

    struct gpu_info
    {
        u32 subgroupSize;
    };

    struct texture_init_desc
    {
        u32 width;
        u32 height;
        texture_format format;
    };
}