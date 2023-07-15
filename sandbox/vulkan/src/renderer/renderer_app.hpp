#pragma once

#include <oblo/core/array_size.hpp>
#include <oblo/core/log.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/render_graph/render_graph.hpp>
#include <oblo/render_graph/render_graph_builder.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/nodes/blit_image_node.hpp>
#include <oblo/vulkan/nodes/deferred.hpp>
#include <oblo/vulkan/nodes/forward.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <sandbox/context.hpp>

namespace oblo::vk
{
    class renderer_app
    {
    public:
        bool init(const sandbox_init_context& context)
        {
            return m_renderer.init({
                .engine = *context.engine,
                .allocator = *context.allocator,
                .frameAllocator = *context.frameAllocator,
                .resourceManager = *context.resourceManager,
            });
        }

        void shutdown(const sandbox_shutdown_context& context)
        {
            m_renderer.shutdown(*context.frameAllocator);
        }

        void first_update(const sandbox_render_context& context)
        {
#if 0
        const auto builder = render_graph_builder<renderer_context>{}
                            .add_node<deferred_gbuffer_node>()
                            .add_node<deferred_lighting_node>()
                            .add_node<blit_image_node>()
                            // .add_edge(&deferred_gbuffer_node::gbuffer, &deferred_lighting_node::gbuffer)
                            .add_input<allocated_buffer>("camera")
                            .add_input<h32<texture>>("final_render_target")
                            .connect(&deferred_gbuffer_node::test, &deferred_lighting_node::test)
                            .connect(&deferred_gbuffer_node::test, &blit_image_node::source)
                            .connect_input<h32<texture>>("final_render_target", &blit_image_node::destination);
#else
            const auto builder = render_graph_builder<renderer_context>{}
                                     .add_node<forward_node>()
                                     .add_input<h32<texture>>("final_render_target")
                                     .connect_input<h32<texture>>("final_render_target", &forward_node::renderTarget);
#endif

            m_graph = m_renderer.create_graph(builder, *context.frameAllocator);

            if (!m_graph)
            {
                log::error("Failed to create render graph");
            }

            init_test_mesh_table();
            update(context);
        }

        void update(const sandbox_render_context& context)
        {
            if (auto* const graph = m_renderer.find_graph(m_graph))
            {
                // Set-up the graph inputs
                auto* const finalRenderTarget = graph->find_input<h32<texture>>("final_render_target");
                OBLO_ASSERT(finalRenderTarget);

                *finalRenderTarget = context.swapchainTexture;
            }

            m_renderer.update({
                .commandBuffer = *context.commandBuffer,
                .frameAllocator = *context.frameAllocator,
            });
        }

        void update_imgui(const sandbox_update_imgui_context&) {}

    private:
        void init_test_mesh_table()
        {
            auto& meshes = m_renderer.get_mesh_table();
            auto& allocator = m_renderer.get_allocator();
            auto& resourceManager = m_renderer.get_resource_manager();
            auto& stringInterner = m_renderer.get_string_interner();
            auto& stagingBuffer = m_renderer.get_staging_buffer();

            constexpr u32 maxVertices{1024};
            constexpr u32 maxIndices{1024};

            const auto position = stringInterner.get_or_add("in_Position");
            const auto normal = stringInterner.get_or_add("in_Normal");
            const auto uv0 = stringInterner.get_or_add("in_UV0");
            const auto color = stringInterner.get_or_add("in_Color");

            const buffer_column_description columns[] = {
                {.name = position, .elementSize = sizeof(vec3)},
                {.name = normal, .elementSize = sizeof(vec3)},
                {.name = uv0, .elementSize = sizeof(vec2)},
                {.name = color, .elementSize = sizeof(vec3)},
            };

            meshes
                .init(columns, allocator, resourceManager, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, maxVertices, maxIndices);

            const mesh_table_entry mesh{
                .id = stringInterner.get_or_add("triangle"),
                .numVertices = 3,
                .numIndices = 3,
            };

            if (!meshes.allocate_meshes({&mesh, 1}))
            {
                return;
            }

            const h32<string> columnSubset[] = {position, color};
            buffer buffers[array_size(columnSubset)];

            meshes.fetch_buffers(resourceManager, columnSubset, buffers, nullptr);

            constexpr vec3 positions[] = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
            constexpr vec3 colors[] = {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

            stagingBuffer.upload(std::as_bytes(std::span{positions}), buffers[0].buffer, buffers[0].offset);
            stagingBuffer.upload(std::as_bytes(std::span{colors}), buffers[1].buffer, buffers[1].offset);
        }

    private:
        renderer m_renderer;
        h32<render_graph> m_graph;
    };
}