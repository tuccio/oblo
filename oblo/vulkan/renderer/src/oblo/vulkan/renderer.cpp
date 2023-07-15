#include <oblo/vulkan/renderer.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/render_graph/render_graph_builder.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/nodes/blit_image_node.hpp>
#include <oblo/vulkan/nodes/deferred.hpp>
#include <oblo/vulkan/nodes/forward.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    bool renderer::init(const renderer::initializer& context)
    {
        m_allocator = &context.allocator;
        m_engine = &context.engine;
        m_resourceManager = &context.resourceManager;

        m_dummy = m_resourceManager->create(
            *m_allocator,
            {
                .size = 16u,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = memory_usage::gpu_only,
            });

        m_stringInterner.init(64);
        m_renderPassManager.init(m_engine->get_device(), m_stringInterner, m_dummy);

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

        renderer_context rendererContext{.renderer = *this, .frameAllocator = context.frameAllocator};
        return m_executor.initialize(&rendererContext);
    }

    void renderer::shutdown(frame_allocator& frameAllocator)
    {
        m_meshes.shutdown(*m_allocator, *m_resourceManager);

        renderer_context rendererContext{.renderer = *this, .frameAllocator = frameAllocator};
        m_executor.shutdown(&rendererContext);

        m_renderPassManager.shutdown();
        m_resourceManager->destroy(*m_allocator, m_dummy);
    }

    void renderer::update(const update_context& context)
    {
        init_test_mesh_table();

        // Set-up the graph inputs
        auto* const finalRenderTarget = m_graph.find_input<h32<texture>>("final_render_target");
        OBLO_ASSERT(finalRenderTarget);

        *finalRenderTarget = context.swapchainTexture;

        renderer_context rendererContext{
            .renderer = *this,
            .frameAllocator = context.frameAllocator,
            .commandBuffer = &context.commandBuffer,
        };

        m_executor.execute(&rendererContext);

        m_stagingBuffer.flush();
    }

    void renderer::init_test_mesh_table()
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
                      *m_allocator,
                      *m_resourceManager,
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

        m_meshes.fetch_buffers(*m_resourceManager, columnSubset, buffers, nullptr);

        constexpr vec3 positions[] = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        constexpr vec3 colors[] = {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

        m_stagingBuffer.init(*m_engine, *m_allocator, 1u << 29);
        m_stagingBuffer.upload(std::as_bytes(std::span{positions}), buffers[0].buffer, buffers[0].offset);
        m_stagingBuffer.upload(std::as_bytes(std::span{colors}), buffers[1].buffer, buffers[1].offset);
    }
}