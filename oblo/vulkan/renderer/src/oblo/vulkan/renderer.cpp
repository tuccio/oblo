#include <oblo/vulkan/renderer.hpp>

#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    struct renderer::wrapped_render_graph : render_graph
    {
    };

    renderer::renderer() = default;
    renderer::~renderer() = default;

    bool renderer::init(const renderer::initializer& context)
    {
        m_vkContext = &context.vkContext;

        m_stagingBuffer.init(get_engine(), get_allocator(), 1u << 27);

        m_dummy = m_vkContext->get_resource_manager().create(get_allocator(),
            {
                .size = 16u,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = memory_usage::gpu_only,
            });

        m_stringInterner.init(64);
        m_renderPassManager.init(m_vkContext->get_device(), m_stringInterner, m_dummy);

        m_drawRegistry.init(*m_vkContext, m_stagingBuffer, m_stringInterner);

        return true;
    }

    void renderer::shutdown()
    {
        auto& allocator = m_vkContext->get_allocator();
        auto& resourceManager = m_vkContext->get_resource_manager();

        m_drawRegistry.shutdown();

        m_renderGraphs.clear();
        m_graphResourcePool.shutdown(*m_vkContext);

        m_renderPassManager.shutdown();
        resourceManager.destroy(allocator, m_dummy);

        m_stagingBuffer.shutdown();
    }

    void renderer::update()
    {
        m_stagingBuffer.flush();

        m_graphResourcePool.begin_build();

        // TODO: Graph dependencies, e.g. shadow maps should run before other graphs
        for (auto& graphData : m_renderGraphs.values())
        {
            m_graphResourcePool.begin_graph();
            graphData.build(m_graphResourcePool);
            m_graphResourcePool.end_graph();
        }

        m_graphResourcePool.end_build(*m_vkContext);

        for (auto& graphData : m_renderGraphs.values())
        {
            graphData.execute(*this, m_graphResourcePool);
        }
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

    stateful_command_buffer& renderer::get_active_command_buffer()
    {
        return m_vkContext->get_active_command_buffer();
    }

    h32<render_graph> renderer::add(render_graph&& graph)
    {
        const h32<render_graph> id{++m_lastRenderGraphId};
        auto [it, ok] = m_renderGraphs.emplace(id, std::move(graph));

        if (!ok)
        {
            return {};
        }

        it->init(*this);
        return id;
    }

    void renderer::remove(h32<render_graph> graph)
    {
        m_renderGraphs.erase(graph);
    }

    render_graph* renderer::find(h32<render_graph> graph)
    {
        return m_renderGraphs.try_find(graph);
    }
}