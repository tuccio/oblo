#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/forward.hpp>

#include <span>

namespace oblo
{
    class instance_data_type_registry;
    class texture_registry;
    struct base_pipeline;
    struct compute_pass;
    struct compute_pass_initializer;
    struct compute_pipeline;
    struct compute_pipeline_initializer;
    struct named_shader_binding;
    struct raytracing_pass;
    struct raytracing_pass_initializer;
    struct raytracing_pipeline;
    struct raytracing_pipeline_initializer;
    struct render_pass;
    struct render_pass_initializer;
    struct render_pipeline;
    struct render_pipeline_initializer;

    struct compute_pass_context;
    struct render_pass_context;
    struct raytracing_pass_context;

    class pass_manager
    {
    public:
        using locate_binding_fn = function_ref<gpu::bindable_object(const named_shader_binding&)>;

    public:
        pass_manager();
        pass_manager(const pass_manager&) = delete;
        pass_manager(pass_manager&&) noexcept = delete;
        pass_manager& operator=(const pass_manager&) = delete;
        pass_manager& operator=(pass_manager&&) noexcept = delete;
        ~pass_manager();

        void init(gpu::gpu_instance& gpu,
            string_interner& interner,
            const texture_registry& textureRegistry,
            const instance_data_type_registry& instanceDataTypeRegistry);

        void shutdown();

        void set_system_include_paths(std::span<const string_view> paths);
        void set_raytracing_enabled(bool isRayTracingEnabled);

        h32<render_pass> register_render_pass(const render_pass_initializer& desc);
        h32<compute_pass> register_compute_pass(const compute_pass_initializer& desc);
        h32<raytracing_pass> register_raytracing_pass(const raytracing_pass_initializer& desc);

        h32<render_pipeline> get_or_create_pipeline(h32<render_pass> handle, const render_pipeline_initializer& desc);
        h32<compute_pipeline> get_or_create_pipeline(h32<compute_pass> handle,
            const compute_pipeline_initializer& desc);
        h32<raytracing_pipeline> get_or_create_pipeline(h32<raytracing_pass> handle,
            const raytracing_pipeline_initializer& desc);

        void begin_frame(hptr<gpu::command_buffer> commandBuffer);
        void end_frame();
        void update_global_descriptor_sets();

        bool is_profiling_enabled() const;
        void set_profiling_enabled(bool enabled);

        [[nodiscard]] expected<render_pass_context> begin_render_pass(hptr<gpu::command_buffer> commandBuffer,
            h32<render_pipeline> pipeline,
            const gpu::graphics_pass_descriptor& renderingInfo) const;

        void end_render_pass(const render_pass_context& context) const;

        expected<compute_pass_context> begin_compute_pass(hptr<gpu::command_buffer> commandBuffer,
            h32<compute_pipeline> pipeline) const;

        void end_compute_pass(const compute_pass_context& context) const;

        expected<raytracing_pass_context> begin_raytracing_pass(hptr<gpu::command_buffer> commandBuffer,
            h32<raytracing_pipeline> pipeline) const;

        void end_raytracing_pass(const raytracing_pass_context& context) const;

        u32 get_subgroup_size() const;

        void push_constants(const render_pass_context& ctx,
            flags<gpu::shader_stage> stages,
            u32 offset,
            std::span<const byte> data) const;

        void push_constants(const compute_pass_context& ctx,
            flags<gpu::shader_stage> stages,
            u32 offset,
            std::span<const byte> data) const;

        void push_constants(const raytracing_pass_context& ctx,
            flags<gpu::shader_stage> stages,
            u32 offset,
            std::span<const byte> data) const;

        void bind_descriptor_sets(const render_pass_context& ctx, locate_binding_fn locateBinding) const;

        void bind_descriptor_sets(const compute_pass_context& ctx, locate_binding_fn locateBinding) const;

        void bind_descriptor_sets(const raytracing_pass_context& ctx, locate_binding_fn locateBinding) const;

        void trace_rays(const raytracing_pass_context& ctx, u32 width, u32 height, u32 depth) const;

        const base_pipeline* get_base_pipeline(const compute_pipeline* pipeline) const;
        const base_pipeline* get_base_pipeline(const render_pipeline* pipeline) const;
        const base_pipeline* get_base_pipeline(const raytracing_pipeline* pipeline) const;

        string_view get_pass_name(const base_pipeline& pipeline) const;

        const string_interner& get_string_interner() const;

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };

    struct render_pass_context
    {
        hptr<gpu::command_buffer> commandBuffer;
        hptr<gpu::graphics_pass> pass;
        const render_pipeline* internalPipeline;
    };

    struct compute_pass_context
    {
        hptr<gpu::command_buffer> commandBuffer;
        hptr<gpu::compute_pass> pass;
        const compute_pipeline* internalPipeline;
    };

    struct raytracing_pass_context
    {
        hptr<gpu::command_buffer> commandBuffer;
        hptr<gpu::raytracing_pass> pass;
        const raytracing_pipeline* internalPipeline;
    };

    struct named_shader_binding
    {
        h32<string> name;
        u32 binding;
        gpu::resource_binding_kind kind;
        flags<gpu::shader_stage> stageFlags;
        bool readOnly;
    };
}
