#include <oblo/graphics/systems/viewport_system.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/graph/topology_builder.hpp>
#include <oblo/vulkan/nodes/debug_triangle_node.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::graphics
{
    namespace
    {
        constexpr std::string_view OutFinalRenderTarget{"Final Render Target"};
        constexpr std::string_view InResolution{"Resolution"};
    }

    struct viewport_system::render_graph_data
    {
        bool isAlive;
        h32<vk::render_graph> id{};
    };

    viewport_system::viewport_system() = default;

    viewport_system::~viewport_system() = default;

    void viewport_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<vk::renderer>();
        OBLO_ASSERT(m_renderer);

        update(ctx);
    }

    void viewport_system::update(const ecs::system_update_context& ctx)
    {
        using namespace oblo::vk;

        for (auto& renderGraphData : m_renderGraphs.values())
        {
            // Set to false to garbage collect
            renderGraphData.isAlive = false;
        }

        for (const auto [entities, viewports] : ctx.entities->range<viewport_component>())
        {
            for (auto&& [entity, viewport] : zip_range(entities, viewports))
            {
                if (!viewport.texture)
                {
                    continue;
                }

                auto* const renderGraphData = m_renderGraphs.try_find(entity);
                render_graph* graph;

                if (!renderGraphData)
                {
                    expected res = topology_builder{}
                                       .add_node<debug_triangle_node>()
                                       .add_output<h32<texture>>(OutFinalRenderTarget)
                                       .add_input<vec2u>(InResolution)
                                       .connect_output(&debug_triangle_node::outRenderTarget, OutFinalRenderTarget)
                                       .connect_input(InResolution, &debug_triangle_node::inResolution)
                                       .build();

                    if (!res)
                    {
                        continue;
                    }

                    const auto [it, ok] = m_renderGraphs.emplace(entity);
                    it->isAlive = true;
                    it->id = m_renderer->add(std::move(*res));

                    graph = m_renderer->find(it->id);
                }
                else
                {
                    renderGraphData->isAlive = true;
                    graph = m_renderer->find(renderGraphData->id);
                }

                if (auto* const resolution = graph->find_input<vec2u>(InResolution))
                {
                    *resolution = vec2u{viewport.width, viewport.height};
                }

                graph->copy_output(OutFinalRenderTarget, viewport.texture);
            }
        }

        if (!m_renderGraphs.empty())
        {
            // TODO: Should implement an iterator for flat_dense_map instead, and erase using that
            auto* const elementsToRemove = allocate_n<ecs::entity>(*ctx.frameAllocator, m_renderGraphs.size());
            u32 numRemovedElements{0};

            for (auto&& [entity, renderGraphData] : zip_range(m_renderGraphs.keys(), m_renderGraphs.values()))
            {
                if (renderGraphData.isAlive)
                {
                    continue;
                }

                m_renderer->remove(renderGraphData.id);
                elementsToRemove[numRemovedElements] = entity;
                ++numRemovedElements;
            }

            for (auto e : std::span(elementsToRemove, numRemovedElements))
            {
                m_renderGraphs.erase(e);
            }
        }

        // TODO: Find a better home for the renderer update
        m_renderer->update();
    }
}