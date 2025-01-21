#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <vulkan/vulkan.h>

#include <span>

namespace oblo
{
    class string;
    class string_interner;

    template <typename T>
    struct flat_key_extractor;

    template <typename Key, typename Value, typename KeyExtractor>
    class flat_dense_map;

    template <typename>
    class function_ref;
}

namespace oblo::vk
{
    class descriptor_set_pool;
    class mesh_table;
    class resource_manager;
    class texture_registry;
    class vulkan_context;
    struct buffer;
    struct bindable_object;
    struct batch_draw_data;
    struct compute_pass;
    struct compute_pass_initializer;
    struct compute_pipeline;
    struct compute_pipeline_initializer;
    struct raytracing_pass;
    struct raytracing_pass_initializer;
    struct raytracing_pipeline;
    struct raytracing_pipeline_initializer;
    struct render_pass;
    struct render_pass_initializer;
    struct render_pipeline;
    struct render_pipeline_initializer;

    using binding_table = flat_dense_map<h32<string>, bindable_object>;

    enum class pipeline_stages : u8;

    struct compute_pass_context;
    struct render_pass_context;
    struct raytracing_pass_context;

    class pass_manager
    {
    public:
        using locate_binding_fn = function_ref<bindable_object(h32<string> name)>;

    public:
        pass_manager();
        pass_manager(const pass_manager&) = delete;
        pass_manager(pass_manager&&) noexcept = delete;
        pass_manager& operator=(const pass_manager&) = delete;
        pass_manager& operator=(pass_manager&&) noexcept = delete;
        ~pass_manager();

        void init(vulkan_context& vkContext,
            string_interner& interner,
            const buffer& dummy,
            const texture_registry& textureRegistry);

        void shutdown(vulkan_context& vkContext);

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

        void update_instance_data_defines(string_view defines);

        bool is_profiling_enabled() const;
        void set_profiling_enabled(bool enabled);

        [[nodiscard]] expected<render_pass_context> begin_render_pass(
            VkCommandBuffer commandBuffer, h32<render_pipeline> pipeline, const VkRenderingInfo& renderingInfo) const;

        void end_render_pass(const render_pass_context& context);

        expected<compute_pass_context> begin_compute_pass(VkCommandBuffer commandBuffer,
            h32<compute_pipeline> pipeline) const;

        void end_compute_pass(const compute_pass_context& context);

        expected<raytracing_pass_context> begin_raytracing_pass(VkCommandBuffer commandBuffer,
            h32<raytracing_pipeline> pipeline) const;

        void end_raytracing_pass(const raytracing_pass_context& context);

        u32 get_subgroup_size() const;

        void push_constants(
            const render_pass_context& ctx, VkShaderStageFlags stages, u32 offset, std::span<const byte> data) const;

        void push_constants(
            const compute_pass_context& ctx, VkShaderStageFlags stages, u32 offset, std::span<const byte> data) const;

        void push_constants(const raytracing_pass_context& ctx,
            VkShaderStageFlags stages,
            u32 offset,
            std::span<const byte> data) const;

        void bind_descriptor_sets(const render_pass_context& ctx,
            std::span<const binding_table* const> bindingTables) const;

        void bind_descriptor_sets(const compute_pass_context& ctx,
            std::span<const binding_table* const> bindingTables) const;

        void bind_descriptor_sets(const compute_pass_context& ctx,
            function_ref<bindable_object(h32<string> name)> locateBinding) const;

        void bind_descriptor_sets(const raytracing_pass_context& ctx,
            std::span<const binding_table* const> bindingTables) const;

        void trace_rays(const raytracing_pass_context& ctx, u32 width, u32 height, u32 depth) const;

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
