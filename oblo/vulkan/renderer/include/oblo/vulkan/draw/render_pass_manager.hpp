#pragma once

#include <oblo/core/handle.hpp>

#include <vulkan/vulkan.h>

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
    struct render_pass;
    struct render_pass_initializer;
    struct render_pipeline;
    struct render_pipeline_initializer;

    using buffer_binding_table = flat_dense_map<h32<string>, buffer, flat_key_extractor<h32<string>>>;

    enum class pipeline_stages : u8;

    struct render_pass_context;

    class render_pass_manager
    {
    public:
        render_pass_manager();
        render_pass_manager(const render_pass_manager&) = delete;
        render_pass_manager(render_pass_manager&&) noexcept = delete;
        render_pass_manager& operator=(const render_pass_manager&) = delete;
        render_pass_manager& operator=(render_pass_manager&&) noexcept = delete;
        ~render_pass_manager();

        void init(const vulkan_context& vkContext,
            string_interner& interner,
            const h32<buffer> dummy,
            const texture_registry& textureRegistry);

        void shutdown(vulkan_context& vkContext);

        h32<render_pass> register_render_pass(const render_pass_initializer& desc);

        h32<render_pipeline> get_or_create_pipeline(h32<render_pass> handle, const render_pipeline_initializer& desc);

        void begin_frame();
        void end_frame();

        [[nodiscard]] bool begin_rendering(render_pass_context& context, const VkRenderingInfo& renderingInfo) const;
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
