#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/frame_graph_resources.hpp>
#include <oblo/renderer/graph/pins.hpp>

#include <span>

namespace oblo
{
    class binding_tables_span;
    class frame_allocator;
    class random_generator;
    class texture;

    template <typename>
    class resource_ptr;

    template <typename E, u32 Size>
    struct flags;

    struct gpu_info;
    struct texture_init_desc;
    struct vec2u;

    using staging_buffer_span = gpu::staging_buffer_span;

    class frame_graph_init_context
    {
    public:
        explicit frame_graph_init_context(frame_graph_impl& frameGraph, const frame_graph_build_args& args);

        h32<compute_pass> register_compute_pass(const compute_pass_initializer& initializer) const;
        h32<render_pass> register_render_pass(const render_pass_initializer& initializer) const;
        h32<raytracing_pass> register_raytracing_pass(const raytracing_pass_initializer& initializer) const;

        const gpu_info& get_gpu_info() const;

    private:
        frame_graph_impl& m_frameGraph;
        const frame_graph_build_args& m_args;
    };

    class frame_graph_build_context
    {
    public:
        explicit frame_graph_build_context(
            frame_graph_impl& frameGraph, frame_graph_build_state& state, const frame_graph_build_args& args);

        [[nodiscard]] h32<compute_pass_instance> compute_pass(h32<compute_pass> pass,
            const compute_pipeline_initializer& initializer) const;

        [[nodiscard]] h32<render_pass_instance> render_pass(h32<render_pass> pass,
            const render_pipeline_initializer& initializer) const;

        [[nodiscard]] h32<raytracing_pass_instance> raytracing_pass(h32<raytracing_pass> pass,
            const raytracing_pipeline_initializer& initializer) const;

        [[nodiscard]] h32<transfer_pass_instance> transfer_pass() const;

        h32<empty_pass_instance> empty_pass() const;

        void create(pin::texture texture, const texture_resource_initializer& initializer, texture_access usage) const;

        void create(pin::buffer buffer, const buffer_resource_initializer& initializer, buffer_access usage) const;

        void create(pin::buffer buffer, const staging_buffer_span& stagedData, buffer_access usage) const;

        h32<retained_texture> create_retained_texture(const texture_resource_initializer& initializer,
            flags<texture_access> usages) const;

        void destroy_retained_texture(h32<retained_texture> handle) const;

        pin::texture get_resource(h32<retained_texture> texture) const;

        void register_texture(pin::texture resource, h32<gpu::image> externalTexture) const;

        // Temporary solution until the acceleration structure is a proper resource.
        void register_global_tlas(h32<gpu::acceleration_structure> accelerationStructure) const;

        void acquire(pin::texture texture, texture_access usage) const;

        h32<resident_texture> acquire_bindless(pin::texture texture, texture_access usage) const;

        h32<resident_texture> load_resource(const resource_ptr<texture>& texture) const;

        void acquire(pin::buffer buffer, buffer_access usage) const;

        void reroute(pin::buffer source, pin::buffer destination) const;
        void reroute(pin::texture source, pin::texture destination) const;

        /// @brief Determines whether the pin contributes to a frame graph output.
        /// @remarks Nodes might decide to skip some computation if certain nodes don't contribute to any frame graph
        /// output.
        bool is_active_output(pin::texture texture) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(pin::buffer buffer) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(pin::texture texture) const;

        [[nodiscard]] pin::buffer create_dynamic_buffer(const buffer_resource_initializer& initializer,
            buffer_access usage) const;

        [[nodiscard]] pin::buffer create_dynamic_buffer(const staging_buffer_span& stagedData,
            buffer_access usage) const;

        template <typename T>
        T& access(pin::data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        std::span<const T> access(pin::data_sink<T> data) const
        {
            return *static_cast<pin::data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        void push(pin::data_sink<T> data, T&& value) const
        {
            auto* a =
                static_cast<pin::data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
            a->push_back(std::move(value));
        }

        template <typename T>
        void push(pin::data_sink<T> data, const T& value) const
        {
            auto* a =
                static_cast<pin::data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
            a->push_back(value);
        }

        expected<texture_init_desc> get_current_initializer(pin::texture texture) const;

        frame_allocator& get_frame_allocator() const;

        random_generator& get_random_generator() const;

        gpu::staging_buffer_span stage_upload(std::span<const byte> data) const;
        gpu::staging_buffer_span stage_upload_image(std::span<const byte> data, u32 texelSize) const;

        u32 get_current_frames_count() const;

        template <typename T>
        bool has_event() const
        {
            return has_event_impl(get_type_id<T>());
        }

        const gpu_info& get_gpu_info() const;

        bool is_recording_metrics() const;

        template <typename T>
        void register_metrics_buffer(pin::buffer b) const
        {
            register_metrics_buffer(get_type_id<T>(), b);
        }

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

        bool has_event_impl(const type_id& type) const;

        void register_metrics_buffer(const type_id& type, pin::buffer b) const;

    private:
        frame_graph_impl& m_frameGraph;
        frame_graph_build_state& m_state;
        const frame_graph_build_args& m_args;
    };

    class frame_graph_execute_context
    {
    public:
        explicit frame_graph_execute_context(const frame_graph_impl& frameGraph,
            frame_graph_execution_state& executeCtx,
            const frame_graph_execute_args& args);

        void begin_pass(h32<frame_graph_pass> handle) const;

        expected<> begin_pass(h32<compute_pass_instance> handle) const;

        expected<> begin_pass(h32<render_pass_instance> handle, const gpu::graphics_pass_descriptor& cfg) const;

        expected<> begin_pass(h32<raytracing_pass_instance> handle) const;

        expected<> begin_pass(h32<transfer_pass_instance> handle) const;

        expected<> begin_pass(h32<empty_pass_instance> handle) const;

        void end_pass() const;

        void bind_descriptor_sets(binding_tables_span bindingTables) const;

        void bind_index_buffer(pin::buffer buffer, u32 bufferOffset, gpu::mesh_index_type indexType) const;

        template <typename T>
        T& access(pin::data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        std::span<const T> access(pin::data_sink<T> data) const
        {
            return *static_cast<pin::data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        pin::acceleration_structure get_global_tlas() const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(pin::buffer buffer) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(pin::texture texture) const;

        /// @brief Queries the number of frames a stable texture has been alive for.
        /// On the first frame of usage the function will return 0.
        /// For transient textures it will always return 0.
        /// @param texture A valid texture resource.
        u32 get_frames_alive_count(pin::texture texture) const;

        /// @brief Queries the number of frames a stable buffer has been alive for.
        /// On the first frame of usage the function will return 0.
        /// For transient buffers it will always return 0.
        /// @param buffer A valid buffer resource.
        u32 get_frames_alive_count(pin::buffer buffer) const;

        u32 get_current_frames_count() const;

        // TODO: This should probably be deprecated, it would be hard to make this thread-safe, staging should happen
        // when building instead.
        void upload(pin::buffer h, std::span<const byte> data, u32 bufferOffset = 0) const;

        void upload(pin::buffer h, const staging_buffer_span& data, u32 bufferOffset = 0) const;

        void upload(pin::texture h, const staging_buffer_span& data) const;

        async_download download(pin::buffer h) const;

        h64<gpu::device_address> get_device_address(pin::buffer buffer) const;

        template <typename T>
        bool has_event() const
        {
            return has_event_impl(get_type_id<T>());
        }

        const gpu_info& get_gpu_info() const;

        void set_viewport(u32 w, u32 h, f32 minDepth = 0.f, f32 maxDepth = 1.f) const;
        void set_scissor(i32 x, i32 y, u32 w, u32 h) const;

        void push_constants(flags<gpu::shader_stage, 14> stages, u32 offset, std::span<const byte> bytes) const;
        void dispatch_compute(u32 groupsX, u32 groupsY, u32 groupsZ) const;
        void trace_rays(u32 x, u32 y, u32 z) const;

        void draw_indexed(u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const;

        void draw_mesh_tasks_indirect_count(pin::buffer drawCallBuffer,
            u32 drawCallBufferOffset,
            pin::buffer drawCallCountBuffer,
            u32 drawCallCountBufferOffset,
            u32 maxDrawCount) const;

        void blit_color(pin::texture srcTexture, pin::texture dstTexture) const;

        vec2u get_resolution(pin::texture h) const;

        bool is_recording_metrics() const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

        bool has_event_impl(const type_id& type) const;

    private:
        const frame_graph_impl& m_frameGraph;
        frame_graph_execution_state& m_state;
        const frame_graph_execute_args& m_args;
    };

    struct gpu_info
    {
        u32 subgroupSize;
    };

    struct texture_init_desc
    {
        u32 width;
        u32 height;
        gpu::image_format format;
    };
}