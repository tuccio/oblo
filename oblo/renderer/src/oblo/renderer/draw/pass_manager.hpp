#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/gpu/gpu_instance.hpp>

#include <vulkan/vulkan_core.h>

#include <span>

namespace oblo
{
    class string;
    class string_interner;

    template <typename T>
    struct flat_key_extractor;

    template <typename>
    class function_ref;
}

namespace oblo
{
    class instance_data_type_registry;
    class texture_registry;
    struct base_pipeline;
    struct bindable_object;
    struct compute_pass;
    struct compute_pass_initializer;
    struct compute_pipeline;
    struct compute_pipeline_initializer;
    struct descriptor_binding;
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
        using locate_binding_fn = function_ref<bindable_object(const descriptor_binding&)>;

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

        void begin_frame(VkCommandBuffer commandBuffer);
        void end_frame();
        void update_global_descriptor_sets();

        bool is_profiling_enabled() const;
        void set_profiling_enabled(bool enabled);

        [[nodiscard]] expected<render_pass_context> begin_render_pass(
            VkCommandBuffer commandBuffer, h32<render_pipeline> pipeline, const VkRenderingInfo& renderingInfo) const;

        void end_render_pass(const render_pass_context& context) const;

        expected<compute_pass_context> begin_compute_pass(VkCommandBuffer commandBuffer,
            h32<compute_pipeline> pipeline) const;

        void end_compute_pass(const compute_pass_context& context) const;

        expected<raytracing_pass_context> begin_raytracing_pass(VkCommandBuffer commandBuffer,
            h32<raytracing_pipeline> pipeline) const;

        void end_raytracing_pass(const raytracing_pass_context& context) const;

        u32 get_subgroup_size() const;

        void push_constants(
            const render_pass_context& ctx, VkShaderStageFlags stages, u32 offset, std::span<const byte> data) const;

        void push_constants(
            const compute_pass_context& ctx, VkShaderStageFlags stages, u32 offset, std::span<const byte> data) const;

        void push_constants(const raytracing_pass_context& ctx,
            VkShaderStageFlags stages,
            u32 offset,
            std::span<const byte> data) const;

        void push_constants(VkCommandBuffer commandBuffer,
            const base_pipeline& pipeline,
            VkShaderStageFlags stages,
            u32 offset,
            std::span<const byte> data) const;

        void bind_descriptor_sets(VkCommandBuffer commandBuffer,
            VkPipelineBindPoint bindPoint,
            const base_pipeline& pipeline,
            locate_binding_fn locateBinding) const;

        void trace_rays(const raytracing_pass_context& ctx, u32 width, u32 height, u32 depth) const;

        const base_pipeline* get_base_pipeline(const compute_pipeline* pipeline) const;
        const base_pipeline* get_base_pipeline(const render_pipeline* pipeline) const;
        const base_pipeline* get_base_pipeline(const raytracing_pipeline* pipeline) const;

        string_view get_pass_name(const base_pipeline& pipeline) const;

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };

    struct render_pass_context
    {
        VkCommandBuffer commandBuffer;
        void* internalCtx;
        const render_pipeline* internalPipeline;
    };

    struct compute_pass_context
    {
        VkCommandBuffer commandBuffer;
        void* internalCtx;
        const compute_pipeline* internalPipeline;
    };

    struct raytracing_pass_context
    {
        VkCommandBuffer commandBuffer;
        void* internalCtx;
        const raytracing_pipeline* internalPipeline;
    };
}
