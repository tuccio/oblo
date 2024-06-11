#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>

#include <vulkan/vulkan.h>

#include <filesystem>
#include <memory>
#include <span>

namespace oblo
{
    struct string;
    class string_interner;

    template <typename T>
    struct flat_key_extractor;

    template <typename Key, typename Value, typename KeyExtractor>
    class flat_dense_map;
}

namespace oblo::vk
{
    class descriptor_set_pool;
    class mesh_table;
    class resource_manager;
    class texture_registry;
    class vulkan_context;
    struct buffer;
    struct batch_draw_data;
    struct compute_pass;
    struct compute_pass_initializer;
    struct compute_pipeline;
    struct compute_pipeline_initializer;
    struct render_pass;
    struct render_pass_initializer;
    struct render_pipeline;
    struct render_pipeline_initializer;

    using buffer_binding_table = flat_dense_map<h32<string>, buffer, flat_key_extractor<h32<string>>>;

    enum class pipeline_stages : u8;

    struct compute_pass_context;
    struct render_pass_context;

    class pass_manager
    {
    public:
        pass_manager();
        pass_manager(const pass_manager&) = delete;
        pass_manager(pass_manager&&) noexcept = delete;
        pass_manager& operator=(const pass_manager&) = delete;
        pass_manager& operator=(pass_manager&&) noexcept = delete;
        ~pass_manager();

        void init(const vulkan_context& vkContext,
            string_interner& interner,
            const buffer& dummy,
            const texture_registry& textureRegistry);

        void shutdown(vulkan_context& vkContext);

        void set_system_include_paths(std::span<const std::filesystem::path> paths);

        h32<render_pass> register_render_pass(const render_pass_initializer& desc);
        h32<compute_pass> register_compute_pass(const compute_pass_initializer& desc);

        h32<render_pipeline> get_or_create_pipeline(h32<render_pass> handle, const render_pipeline_initializer& desc);
        h32<compute_pipeline> get_or_create_pipeline(h32<compute_pass> handle,
            const compute_pipeline_initializer& desc);

        void begin_frame();
        void end_frame();

        [[nodiscard]] expected<render_pass_context> begin_render_pass(
            VkCommandBuffer commandBuffer, h32<render_pipeline> pipeline, const VkRenderingInfo& renderingInfo) const;

        void end_render_pass(const render_pass_context& context);

        void draw(const render_pass_context& context,
            std::span<const buffer> batchDrawCommands,
            std::span<const batch_draw_data> batchDrawData,
            std::span<const buffer_binding_table> perDrawBindingTable,
            std::span<const buffer_binding_table* const> bindingTables = {});

        expected<compute_pass_context> begin_compute_pass(VkCommandBuffer commandBuffer,
            h32<compute_pipeline> pipeline) const;

        void end_compute_pass(const compute_pass_context& context);

        void dispatch(const compute_pass_context& context,
            u32 x,
            u32 y,
            u32 z,
            std::span<const buffer_binding_table* const> bindingTables = {});

        u32 get_subgroup_size() const;

    private:
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };

    struct render_pass_context
    {
        VkCommandBuffer commandBuffer;
        const render_pipeline* internalPipeline;
    };

    struct compute_pass_context
    {
        VkCommandBuffer commandBuffer;
        const compute_pipeline* internalPipeline;
    };
}
