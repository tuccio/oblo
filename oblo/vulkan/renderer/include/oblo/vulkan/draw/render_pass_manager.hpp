#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/shader_compiler.hpp>

#include <memory>

namespace oblo
{
    class string_interner;
}

namespace oblo::vk
{
    class mesh_table;
    class resource_manager;
    struct buffer;
    struct render_pass;
    struct render_pass_initializer;
    struct render_pipeline;
    struct render_pipeline_initializer;

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

        void init(VkDevice device, string_interner& interner, const h32<buffer> dummy);
        void shutdown();

        h32<render_pass> register_render_pass(const render_pass_initializer& desc);

        h32<render_pipeline> get_or_create_pipeline(h32<render_pass> handle, const render_pipeline_initializer& desc);

        [[nodiscard]] bool begin_rendering(render_pass_context& context, const VkRenderingInfo& renderingInfo) const;
        void end_rendering(const render_pass_context& context);

        void bind(
            const render_pass_context& context, const resource_manager& resourceManager, const mesh_table& meshTable);

    private:
        struct file_watcher;

    private:
        frame_allocator m_frameAllocator;
        VkDevice m_device{};
        u32 m_lastRenderPassId{};
        u32 m_lastRenderPipelineId{};
        flat_dense_map<h32<render_pass>, render_pass> m_renderPasses;
        flat_dense_map<h32<render_pipeline>, render_pipeline> m_renderPipelines;
        string_interner* m_interner{nullptr};
        h32<buffer> m_dummy{};
        std::unique_ptr<file_watcher> m_fileWatcher;
    };

    struct render_pass_context
    {
        VkCommandBuffer commandBuffer;
        h32<render_pipeline> pipeline;
        const render_pipeline* internalPipeline;
    };
}
