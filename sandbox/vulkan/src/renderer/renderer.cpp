#include <renderer/renderer.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/render_graph/render_graph_builder.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/texture.hpp>
#include <renderer/nodes/blit_image_node.hpp>
#include <renderer/nodes/deferred.hpp>
#include <renderer/nodes/forward.hpp>
#include <renderer/renderer_context.hpp>
#include <sandbox/context.hpp>

namespace oblo::vk
{
    bool renderer::init(const sandbox_init_context& context)
    {
        m_dummy = context.resourceManager->create(
            *context.allocator,
            {
                .size = 16u,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = memory_usage::gpu_only,
            });

        m_stringInterner.init(64);
        m_renderPassManager.init(context.engine->get_device(), m_stringInterner, m_dummy);

#if 0
        const auto ec = render_graph_builder<renderer_context>{}
                            .add_node<deferred_gbuffer_node>()
                            .add_node<deferred_lighting_node>()
                            .add_node<blit_image_node>()
                            // .add_edge(&deferred_gbuffer_node::gbuffer, &deferred_lighting_node::gbuffer)
                            .add_input<allocated_buffer>("camera")
                            .add_input<h32<texture>>("final_render_target")
                            .connect(&deferred_gbuffer_node::test, &deferred_lighting_node::test)
                            .connect(&deferred_gbuffer_node::test, &blit_image_node::source)
                            .connect_input<h32<texture>>("final_render_target", &blit_image_node::destination)
                            .build(m_graph, m_executor);
#else
        const auto ec = render_graph_builder<renderer_context>{}
                            .add_node<forward_node>()
                            .add_input<h32<texture>>("final_render_target")
                            .connect_input<h32<texture>>("final_render_target", &forward_node::renderTarget)
                            .build(m_graph, m_executor);
#endif

        if (ec)
        {
            return false;
        }

        m_state = {
            .stringInterner = &m_stringInterner,
            .renderPassManager = &m_renderPassManager,
            .meshTable = &m_meshes,
        };

        renderer_context rendererContext{.initContext = &context, .state = m_state};
        return m_executor.initialize(&rendererContext);
    }

    void renderer::shutdown(const sandbox_shutdown_context& context)
    {
        m_meshes.shutdown(*context.allocator, *context.resourceManager);

        renderer_context rendererContext{.shutdownContext = &context, .state = m_state};
        m_executor.shutdown(&rendererContext);

        m_renderPassManager.shutdown();
        context.resourceManager->destroy(*context.allocator, m_dummy);
    }

    void renderer::update(const sandbox_render_context& context)
    {
        init_test_mesh_table(context);

        // Set-up the graph inputs
        auto* const finalRenderTarget = m_graph.find_input<h32<texture>>("final_render_target");
        OBLO_ASSERT(finalRenderTarget);

        *finalRenderTarget = context.swapchainTexture;

        renderer_context rendererContext{.renderContext = &context, .state = m_state};
        m_executor.execute(&rendererContext);

        m_state.lastFrameHeight = context.height;
        m_state.lastFrameWidth = context.width;

        m_stagingBuffer.flush();
    }

    void renderer::update_imgui(const sandbox_update_imgui_context&) {}

    void renderer::init_test_mesh_table(const sandbox_render_context& context)
    {
        if (!m_meshes.vertex_attribute_buffers().empty())
        {
            return;
        }

        constexpr u32 maxVertices{1024};
        constexpr u32 maxIndices{1024};

        const auto position = m_stringInterner.get_or_add("in_Position");
        const auto normal = m_stringInterner.get_or_add("in_Normal");
        const auto uv0 = m_stringInterner.get_or_add("in_UV0");
        const auto color = m_stringInterner.get_or_add("in_Color");

        const buffer_column_description columns[] = {
            {.name = position, .elementSize = sizeof(vec3)},
            {.name = normal, .elementSize = sizeof(vec3)},
            {.name = uv0, .elementSize = sizeof(vec2)},
            {.name = color, .elementSize = sizeof(vec3)},
        };

        m_meshes.init(columns,
                      *context.allocator,
                      *context.resourceManager,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      maxVertices,
                      maxIndices);

        const mesh_table_entry mesh{
            .id = m_stringInterner.get_or_add("triangle"),
            .numVertices = 3,
            .numIndices = 3,
        };

        if (!m_meshes.allocate_meshes({&mesh, 1}))
        {
            return;
        }

        const h32<string> columnSubset[] = {position, color};
        buffer buffers[array_size(columnSubset)];

        m_meshes.fetch_buffers(*context.resourceManager, columnSubset, buffers, nullptr);

        constexpr vec3 positions[] = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        constexpr vec3 colors[] = {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

        m_stagingBuffer.init(*context.engine, *context.allocator, 1u << 29);
        m_stagingBuffer.upload(std::as_bytes(std::span{positions}), buffers[0].buffer, buffers[0].offset);
        m_stagingBuffer.upload(std::as_bytes(std::span{colors}), buffers[1].buffer, buffers[1].offset);
    }
}