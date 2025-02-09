#include <oblo/graphics/systems/viewport_system.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/thread/async_download.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/graphics/systems/scene_renderer.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/view_projection.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/data/time_buffer.hpp>
#include <oblo/vulkan/data/visibility_debug_mode.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/templates/graph_templates.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo
{
    namespace
    {
        void apply_viewport_mode(
            vk::frame_graph& frameGraph, h32<vk::frame_graph_subgraph> subgraph, viewport_mode mode);
    }

    struct viewport_system::render_graph_data
    {
        bool isAlive{};
        h32<vk::frame_graph_subgraph> subgraph{};
        h32<vk::texture> texture{};
        u32 width{};
        u32 height{};

        mat4 lastFrameViewProj;
    };

    viewport_system::viewport_system() = default;

    viewport_system::~viewport_system()
    {
        if (!m_renderer)
        {
            return;
        }

        for (auto& graphData : m_renderGraphs.values())
        {
            m_renderer->get_frame_graph().remove(graphData.subgraph);
        }
    }

    void viewport_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<vk::renderer>();
        OBLO_ASSERT(m_renderer);

        m_sceneRenderer = ctx.services->find<scene_renderer>();
        OBLO_ASSERT(m_sceneRenderer);
        m_sceneRenderer->ensure_setup(*ctx.entities);

        update(ctx);
    }

    void viewport_system::update(const ecs::system_update_context& ctx)
    {
        using namespace oblo::vk;

        auto& frameGraph = m_renderer->get_frame_graph();

        for (auto& renderGraphData : m_renderGraphs.values())
        {
            // Set to false to garbage collect
            renderGraphData.isAlive = false;
        }

        for (auto&& chunk : ctx.entities->range<global_transform_component, camera_component, viewport_component>())
        {
            for (auto&& [entity, transform, camera, viewport] :
                chunk.zip<ecs::entity, global_transform_component, camera_component, viewport_component>())
            {
                auto* renderGraphData = m_renderGraphs.try_find(entity);

                bool isFirstFrame = false;

                if (!renderGraphData)
                {
                    const auto [it, ok] = m_renderGraphs.emplace(entity);
                    it->isAlive = true;

                    const auto registry = vk::create_frame_graph_registry();

                    const auto mainViewTemplate = vk::main_view::create(registry,
                        {
                            .withPicking = true,
                            .withSurfelsGI = true,
                        });

                    const auto subgraph = frameGraph.instantiate(mainViewTemplate);
                    it->subgraph = subgraph;
                    viewport.graph = subgraph;

                    m_sceneRenderer->add_scene_view(subgraph);

                    renderGraphData = &*it;

                    isFirstFrame = true;
                }
                else
                {
                    renderGraphData->isAlive = true;
                }

                const u32 renderWidth = max(viewport.width, 1u);
                const u32 renderHeight = max(viewport.height, 1u);

                frameGraph
                    .set_input(renderGraphData->subgraph, main_view::InResolution, vec2u{renderWidth, renderHeight})
                    .assert_value();

                frameGraph
                    .set_input(renderGraphData->subgraph,
                        main_view::InTime,
                        time_buffer{
                            .frameIndex = m_frameIndex,
                        })
                    .assert_value();

                apply_viewport_mode(frameGraph, renderGraphData->subgraph, viewport.mode);

                {
                    // TODO: Deal with errors, also transposing would be enough here most likely
                    const mat4 view = inverse(transform.localToWorld).assert_value_or(mat4::identity());

                    const f32 ratio = f32(viewport.height) / viewport.width;
                    const mat4 proj = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

                    const mat4 viewProj = proj * view;
                    const mat4 invViewProj = inverse(viewProj).assert_value_or(mat4::identity());
                    const mat4 invProj = inverse(proj).assert_value_or(mat4::identity());

                    const vec4 position = transform.localToWorld.columns[3];

                    const camera_buffer cameraBuffer{
                        .view = view,
                        .projection = proj,
                        .viewProjection = viewProj,
                        .invViewProjection = invViewProj,
                        .invProjection = invProj,
                        .lastFrameViewProjection = isFirstFrame ? viewProj : renderGraphData->lastFrameViewProj,
                        .frustum = make_frustum_from_inverse_view_projection(invViewProj),
                        .position = {position.x, position.y, position.z},
                    };

                    frameGraph.set_input(renderGraphData->subgraph, main_view::InCamera, cameraBuffer).assert_value();

                    renderGraphData->lastFrameViewProj = viewProj;
                }

                {
                    picking_configuration pickingConfig{};

                    frameGraph.set_output_state(renderGraphData->subgraph, main_view::OutPicking, false);

                    switch (viewport.picking.state)
                    {
                    case picking_request::state::requested: {
                        pickingConfig = {
                            .coordinates = viewport.picking.coordinates,
                        };

                        viewport.picking.state = picking_request::state::awaiting;
                        frameGraph.set_output_state(renderGraphData->subgraph, main_view::OutPicking, true);
                    }

                    break;

                    case picking_request::state::awaiting: {
                        const expected asyncDownload =
                            frameGraph.get_output<async_download>(renderGraphData->subgraph, main_view::OutPicking);

                        if (!asyncDownload)
                        {
                            viewport.picking.state = picking_request::state::failed;
                        }
                        else
                        {
                            const expected downloadResult = (*asyncDownload)->try_get_result();

                            if (downloadResult)
                            {
                                const std::span bytes = *downloadResult;
                                std::memcpy(&viewport.picking.result, bytes.data(), min(bytes.size(), sizeof(u32)));
                                viewport.picking.state = picking_request::state::served;
                            }
                            else
                            {
                                switch (downloadResult.error())
                                {
                                case async_download::error::not_ready:
                                    break;
                                case async_download::error::uninitialized:
                                    viewport.picking.state = picking_request::state::failed;
                                    break;
                                case async_download::error::broken_promise:
                                    viewport.picking.state = picking_request::state::failed;
                                    break;
                                }
                            }
                        }
                    }

                    break;

                    default:
                        break;
                    }

                    frameGraph
                        .set_input(renderGraphData->subgraph,
                            main_view::InPickingConfiguration,
                            std::move(pickingConfig))
                        .assert_value();
                }
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

                m_renderer->get_frame_graph().remove(renderGraphData.subgraph);
                m_sceneRenderer->remove_scene_view(renderGraphData.subgraph);

                elementsToRemove[numRemovedElements] = entity;
                ++numRemovedElements;
            }

            for (auto e : std::span(elementsToRemove, numRemovedElements))
            {
                m_renderGraphs.erase(e);
            }
        }

        ++m_frameIndex;
    }

    namespace
    {
        void apply_viewport_mode(
            vk::frame_graph& frameGraph, h32<vk::frame_graph_subgraph> subgraph, viewport_mode mode)
        {
            frameGraph.disable_all_outputs(subgraph);

            switch (mode)
            {
            case viewport_mode::lit:
                frameGraph.set_output_state(subgraph, vk::main_view::OutLitImage, true);
                break;

            case viewport_mode::albedo:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::albedo)
                    .assert_value();
                break;

            case viewport_mode::normal_map:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::normal_map)
                    .assert_value();
                break;

            case viewport_mode::normals:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::normals)
                    .assert_value();
                break;

            case viewport_mode::tangents:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::tangents)
                    .assert_value();
                break;

            case viewport_mode::bitangents:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::bitangents)
                    .assert_value();
                break;

            case viewport_mode::uv0:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::uv0)
                    .assert_value();
                break;

            case viewport_mode::meshlet:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::meshlet)
                    .assert_value();
                break;

            case viewport_mode::metalness:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::metalness)
                    .assert_value();
                break;

            case viewport_mode::roughness:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::roughness)
                    .assert_value();
                break;

            case viewport_mode::emissive:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::emissive)
                    .assert_value();
                break;

            case viewport_mode::motion_vectors:
                frameGraph.set_output_state(subgraph, vk::main_view::OutDebugImage, true);
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::motion_vectors)
                    .assert_value();
                break;

            case viewport_mode::raytracing_debug:
                frameGraph.set_output_state(subgraph, vk::main_view::OutRTDebugImage, true);
                break;

            case viewport_mode::gi_surfels:
                frameGraph.set_output_state(subgraph, vk::main_view::OutGISurfelsImage, true);
                break;

            case viewport_mode::gi_surfels_lighting:
                frameGraph.set_output_state(subgraph, vk::main_view::OutGiSurfelsLightingImage, true);
                break;

            case viewport_mode::gi_surfels_raycount:
                frameGraph.set_output_state(subgraph, vk::main_view::OutGiSurfelsRayCount, true);
                break;

            case viewport_mode::gi_surfels_inconsistency:
                frameGraph.set_output_state(subgraph, vk::main_view::OutGiSurfelsInconsistency, true);
                break;

            default:
                frameGraph.set_output_state(subgraph, vk::main_view::OutLitImage, true);
                unreachable();
                break;
            }
        }
    }
}