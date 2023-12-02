#pragma once

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
    class draw_registry;
    class mesh_table;
    class resource_manager;
    class texture_registry;
    class vulkan_context;
    struct buffer;
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
            const h32<buffer> dummy,
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

        [[nodiscard]] bool begin_render(render_pass_context& context, const VkRenderingInfo& renderingInfo) const;
        void end_rendering(const render_pass_context& context);

        void draw(const render_pass_context& context,
            const resource_manager& resourceManager,
            const draw_registry& drawRegistry,
            std::span<const buffer_binding_table* const> bindingTables = {});

    private:
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };

    struct render_pass_context
    {
        VkCommandBuffer commandBuffer;
        h32<render_pipeline> pipeline;
        const render_pipeline* internalPipeline;
    };
}
