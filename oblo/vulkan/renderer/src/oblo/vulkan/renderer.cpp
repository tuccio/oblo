#include <oblo/vulkan/renderer.hpp>

#include <oblo/render_graph/render_graph.hpp>
#include <oblo/render_graph/render_graph_builder.hpp>
#include <oblo/render_graph/render_graph_seq_executor.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    struct renderer::render_graph_data
    {
        render_graph graph;
        render_graph_seq_executor executor;
    };

    renderer::renderer() = default;
    renderer::~renderer() = default;

    bool renderer::init(const renderer::initializer& context)
    {
        m_vkContext = &context.vkContext;

        m_stagingBuffer.init(get_engine(), get_allocator(), 1u << 29);

        m_dummy = m_vkContext->get_resource_manager().create(
            get_allocator(),
            {
                .size = 16u,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = memory_usage::gpu_only,
            });

        m_stringInterner.init(64);
        m_renderPassManager.init(m_vkContext->get_device(), m_stringInterner, m_dummy);

        return true;
    }

    void renderer::shutdown(frame_allocator& frameAllocator)
    {
        auto& allocator = m_vkContext->get_allocator();
        auto& resourceManager = m_vkContext->get_resource_manager();

        m_meshes.shutdown(allocator, resourceManager);

        renderer_context rendererContext{.renderer = *this, .frameAllocator = frameAllocator};

        for (auto& [graph, executor] : m_renderGraphs.values())
        {
            executor.shutdown(&rendererContext);
        }

        m_renderGraphs.clear();

        m_renderPassManager.shutdown();
        resourceManager.destroy(allocator, m_dummy);

        m_stagingBuffer.shutdown();
    }

    void renderer::update(const update_context& context)
    {
        renderer_context rendererContext{
            .renderer = *this,
            .frameAllocator = context.frameAllocator,
            .commandBuffer = &m_vkContext->get_active_command_buffer(),
        };

        for (auto& [graph, executor] : m_renderGraphs.values())
        {
            executor.execute(&rendererContext);
        }

        m_stagingBuffer.flush();
    }

    h32<render_graph> renderer::create_graph(const render_graph_builder<renderer_context>& builder,
                                             frame_allocator& frameAllocator)
    {
        render_graph graph;
        render_graph_seq_executor executor;

        if (builder.build(graph, executor))
        {
            return {};
        }

        renderer_context rendererContext{
            .renderer = *this,
            .frameAllocator = frameAllocator,
        };

        if (!executor.initialize(&rendererContext))
        {
            return {};
        }

        h32<render_graph> newHandle{++m_lastRenderGraphId};
        m_renderGraphs.emplace(newHandle, std::move(graph), std::move(executor));
        return newHandle;
    }

    void renderer::destroy_graph(h32<render_graph> handle, frame_allocator& frameAllocator)
    {
        auto* const graph = m_renderGraphs.try_find(handle);

        if (graph)
        {
            renderer_context rendererContext{.renderer = *this, .frameAllocator = frameAllocator};
            graph->executor.shutdown(&rendererContext);
            m_renderGraphs.erase(handle);
        }
    }

    render_graph* renderer::find_graph(h32<render_graph> handle)
    {
        auto* const graph = m_renderGraphs.try_find(handle);
        return graph ? &graph->graph : nullptr;
    }

    single_queue_engine& renderer::get_engine()
    {
        return m_vkContext->get_engine();
    }

    allocator& renderer::get_allocator()
    {
        return m_vkContext->get_allocator();
    }

    resource_manager& renderer::get_resource_manager()
    {
        return m_vkContext->get_resource_manager();
    }
}