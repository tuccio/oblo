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
#include <oblo/graphics/services/scene_renderer.hpp>
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
        u32 width{};
        u32 height{};

        mat4 lastFrameViewProj;
    };

    viewport_system::viewport_system() = default;

    viewport_system::~viewport_system() = default;

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
                if (!viewport.graph)
                {
                    continue;
                }

                auto* renderGraphData = m_renderGraphs.try_find(entity);

                bool isFirstFrame = false;

                if (!renderGraphData)
                {
                    const auto [it, ok] = m_renderGraphs.emplace(entity);
                    it->isAlive = true;

                    renderGraphData = &*it;

                    isFirstFrame = true;
                }
                else
                {
                    renderGraphData->isAlive = true;
                }

                const u32 renderWidth = max(viewport.width, 1u);
                const u32 renderHeight = max(viewport.height, 1u);

                frameGraph.set_input(viewport.graph, main_view::InResolution, vec2u{renderWidth, renderHeight})
                    .assert_value();

                frameGraph
                    .set_input(viewport.graph,
                        main_view::InTime,
                        time_buffer{
                            .frameIndex = m_frameIndex,
                        })
                    .assert_value();

                apply_viewport_mode(frameGraph, viewport.graph, viewport.mode);

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

                    frameGraph.set_input(viewport.graph, main_view::InCamera, cameraBuffer).assert_value();

                    renderGraphData->lastFrameViewProj = viewProj;
                }

                {
                    picking_configuration pickingConfig{};

                    frameGraph.set_output_state(viewport.graph, main_view::OutPicking, false);

                    switch (viewport.picking.state)
                    {
                    case picking_request::state::requested: {
                        pickingConfig = {
                            .coordinates = viewport.picking.coordinates,
                        };

                        viewport.picking.state = picking_request::state::awaiting;
                        frameGraph.set_output_state(viewport.graph, main_view::OutPicking, true);
                    }

                    break;

                    case picking_request::state::awaiting: {
                        const expected asyncDownload =
                            frameGraph.get_output<async_download>(viewport.graph, main_view::OutPicking);

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

                    frameGraph.set_input(viewport.graph, main_view::InPickingConfiguration, std::move(pickingConfig))
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
            switch (mode)
            {
            case viewport_mode::lit:
                break;

            case viewport_mode::albedo:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::albedo)
                    .assert_value();
                break;

            case viewport_mode::normal_map:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::normal_map)
                    .assert_value();
                break;

            case viewport_mode::normals:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::normals)
                    .assert_value();
                break;

            case viewport_mode::tangents:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::tangents)
                    .assert_value();
                break;

            case viewport_mode::bitangents:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::bitangents)
                    .assert_value();
                break;

            case viewport_mode::uv0:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::uv0)
                    .assert_value();
                break;

            case viewport_mode::meshlet:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::meshlet)
                    .assert_value();
                break;

            case viewport_mode::metalness:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::metalness)
                    .assert_value();
                break;

            case viewport_mode::roughness:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::roughness)
                    .assert_value();
                break;

            case viewport_mode::emissive:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::emissive)
                    .assert_value();
                break;

            case viewport_mode::motion_vectors:
                frameGraph.set_input(subgraph, vk::main_view::InDebugMode, vk::visibility_debug_mode::motion_vectors)
                    .assert_value();
                break;

            case viewport_mode::raytracing_debug:
                break;

            case viewport_mode::gi_surfels:
                break;

            case viewport_mode::gi_surfels_lighting:
                break;

            case viewport_mode::gi_surfels_raycount:
                break;

            case viewport_mode::gi_surfels_inconsistency:
                break;

            default:
                unreachable();
                break;
            }
        }
    }

    string_view get_viewport_mode_graph_output(viewport_mode mode)
    {
        switch (mode)
        {
        case viewport_mode::lit:
            return vk::main_view::OutLitImage;

        case viewport_mode::albedo:
            return vk::main_view::OutDebugImage;

        case viewport_mode::normal_map:
            return vk::main_view::OutDebugImage;

        case viewport_mode::normals:
            return vk::main_view::OutDebugImage;

        case viewport_mode::tangents:
            return vk::main_view::OutDebugImage;

        case viewport_mode::bitangents:
            return vk::main_view::OutDebugImage;

        case viewport_mode::uv0:
            return vk::main_view::OutDebugImage;

        case viewport_mode::meshlet:
            return vk::main_view::OutDebugImage;

        case viewport_mode::metalness:
            return vk::main_view::OutDebugImage;

        case viewport_mode::roughness:
            return vk::main_view::OutDebugImage;

        case viewport_mode::emissive:
            return vk::main_view::OutDebugImage;

        case viewport_mode::motion_vectors:
            return vk::main_view::OutDebugImage;

        case viewport_mode::raytracing_debug:
            return vk::main_view::OutRTDebugImage;

        case viewport_mode::gi_surfels:
            return vk::main_view::OutGISurfelsImage;

        case viewport_mode::gi_surfels_lighting:
            return vk::main_view::OutGiSurfelsLightingImage;

        case viewport_mode::gi_surfels_raycount:
            return vk::main_view::OutGiSurfelsRayCount;

        case viewport_mode::gi_surfels_inconsistency:
            return vk::main_view::OutGiSurfelsInconsistency;

        default:
            unreachable();
        }
    }
}