#include <gtest/gtest.h>

#include <oblo/core/array_size.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/sandbox/sandbox_app.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/graph/topology_builder.hpp>
#include <oblo/vulkan/render_pass_initializer.hpp>
#include <oblo/vulkan/render_pass_manager.hpp>
#include <oblo/vulkan/renderer.hpp>

namespace oblo::vk::test
{
    namespace
    {
        struct fill_depth_node
        {
            resource<texture> outDepthBuffer;
            data<vec2u> inResolution;

            void build(runtime_builder& builder)
            {
                const auto resolution = builder.access(inResolution);

                builder.create(outDepthBuffer,
                               {
                                   .width = resolution.x,
                                   .height = resolution.y,
                                   .format = VK_FORMAT_D24_UNORM_S8_UINT,
                                   .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                   .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                               },
                               resource_usage::depth_stencil_write);
            }
        };

        struct fill_color_node
        {
            resource<texture> outRenderTarget;
            resource<texture> inDepthBuffer;
            data<vec2u> inResolution;

            h32<render_pass> renderPass;

            void build(runtime_builder& builder)
            {
                const auto resolution = builder.access(inResolution);

                builder.create(outRenderTarget,
                               {
                                   .width = resolution.x,
                                   .height = resolution.y,
                                   .format = VK_FORMAT_R8G8B8A8_UNORM,
                                   .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               },
                               resource_usage::render_target_write);

                builder.use(inDepthBuffer, resource_usage::depth_stencil_read);
            }

            void execute(const runtime_context& context)
            {
                auto& renderPassManager = context.get_render_pass_manager();

                if (!renderPass)
                {
                    renderPass = renderPassManager.register_render_pass({
                        .name = "fill_color_node",
                        .stages =
                            {
                                {
                                    .stage = pipeline_stages::vertex,
                                    .shaderSourcePath = OBLO_TEST_RESOURCES "/shaders/basic.vert",
                                },
                                {
                                    .stage = pipeline_stages::fragment,
                                    .shaderSourcePath = OBLO_TEST_RESOURCES "/shaders/basic.frag",
                                },
                            },
                    });
                }

                const VkCommandBuffer commandBuffer = context.get_command_buffer();

                const auto renderTarget = context.access(outRenderTarget);
                const auto depthBuffer = context.access(inDepthBuffer);

                auto& frameAllocator = context.get_frame_allocator();

                const auto pipeline = renderPassManager.get_or_create_pipeline(
                    frameAllocator,
                    renderPass,
                    {
                        .renderTargets =
                            {
                                .colorAttachmentFormats = {renderTarget.initializer.format},
                                .depthFormat = depthBuffer.initializer.format,
                            },
                    });

                render_pass_context renderPassContext{
                    .commandBuffer = commandBuffer,
                    .pipeline = pipeline,
                    .frameAllocator = frameAllocator,
                };

                const VkRenderingAttachmentInfo colorAttachment{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = renderTarget.view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                };

                const VkRenderingAttachmentInfo depthAttachment{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = depthBuffer.view,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .storeOp = VK_ATTACHMENT_STORE_OP_NONE,
                };

                const auto [renderWidth, renderHeight, _] = renderTarget.initializer.extent;

                const VkRenderingInfo renderInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea = {.extent{.width = renderWidth, .height = renderHeight}},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &colorAttachment,
                    .pDepthAttachment = &depthAttachment,
                };

                renderPassManager.begin_rendering(renderPassContext, renderInfo);

                {
                    const VkViewport viewport{
                        .width = f32(renderWidth),
                        .height = f32(renderHeight),
                        .minDepth = 0.f,
                        .maxDepth = 1.f,
                    };

                    const VkRect2D scissor{.extent{.width = renderWidth, .height = renderHeight}};

                    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
                }

                const auto& meshTable = context.get_mesh_table();
                const auto& resourceManager = context.get_resource_manager();
                renderPassManager.bind(renderPassContext, resourceManager, meshTable);

                vkCmdDraw(commandBuffer, meshTable.vertex_count(), meshTable.meshes_count(), 0, 0);

                renderPassManager.end_rendering(renderPassContext);
            }
        };

        struct copy_node
        {
            resource<texture> inSource;
            resource<texture> outTarget;

            data<vec2u> inResolution;

            void build(runtime_builder& builder)
            {
                // TODO: Actually get resolution and format from source somehow
                const auto resolution = builder.access(inResolution);

                builder.use(inSource, resource_usage::shader_read);
                builder.create(outTarget,
                               {
                                   .width = resolution.x,
                                   .height = resolution.y,
                                   .format = VK_FORMAT_R8G8B8A8_UNORM,
                                   .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               },
                               resource_usage::render_target_write);
            }
        };

        void init_test_mesh_table(renderer& renderer, frame_allocator& frameAllocator)
        {
            auto& meshes = renderer.get_mesh_table();
            auto& allocator = renderer.get_allocator();
            auto& resourceManager = renderer.get_resource_manager();
            auto& stringInterner = renderer.get_string_interner();

            constexpr u32 maxVertices{1024};
            constexpr u32 maxIndices{1024};

            const auto position = stringInterner.get_or_add("in_Position");

            const buffer_column_description columns[] = {
                {.name = position, .elementSize = sizeof(vec3)},
            };

            const bool meshTableCreated =
                meshes.init(frameAllocator,
                            columns,
                            allocator,
                            resourceManager,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            maxVertices,
                            maxIndices);

            OBLO_ASSERT(meshTableCreated);

            const mesh_table_entry mesh{
                .id = stringInterner.get_or_add("quad"),
                .numVertices = 6,
                .numIndices = 6,
            };

            if (!meshes.allocate_meshes({&mesh, 1}))
            {
                return;
            }

            const h32<string> columnSubset[] = {position};
            buffer buffers[array_size(columnSubset)];

            meshes.fetch_buffers(resourceManager, columnSubset, buffers, nullptr);

            constexpr vec3 positions[] = {
                {-1.f, -1.f, 0.5f},
                {-1.f, 1.f, 0.5f},
                {1.f, 1.f, 0.5f},
                {-1.f, -1.f, 0.5f},
                {1.f, 1.f, 0.5f},
                {1.f, -1.f, 0.5f},
            };

            auto& stagingBuffer = renderer.get_staging_buffer();

            stagingBuffer.upload(std::as_bytes(std::span{positions}), buffers[0].buffer, buffers[0].offset);
        }

        struct render_graph_test
        {
            bool init(const vk::sandbox_init_context& ctx)
            {
                if (!renderer.init({
                        .vkContext = *ctx.vkContext,
                        .frameAllocator = *ctx.frameAllocator,
                    }))
                {
                    return false;
                }

                init_test_mesh_table(renderer, *ctx.frameAllocator);

                expected res = topology_builder{}
                                   .add_node<fill_depth_node>()
                                   .add_node<fill_color_node>()
                                   .add_node<copy_node>()
                                   .add_input<vec2u>("RenderResolution")
                                   .add_output<h32<texture>>("FinalRender")
                                   .connect_input("RenderResolution", &fill_depth_node::inResolution)
                                   .connect_input("RenderResolution", &fill_color_node::inResolution)
                                   .connect_input("RenderResolution", &copy_node::inResolution)
                                   .connect(&fill_depth_node::outDepthBuffer, &fill_color_node::inDepthBuffer)
                                   .connect(&fill_color_node::outRenderTarget, &copy_node::inSource)
                                   .connect_output(&copy_node::outTarget, "FinalRender")
                                   .build();

                if (!res)
                {
                    return false;
                }

                graph = std::move(*res);
                return true;
            }

            void shutdown(const vk::sandbox_shutdown_context& ctx)
            {
                resourcePool.shutdown(*ctx.vkContext);
                renderer.shutdown(*ctx.frameAllocator);
            }

            void update(const vk::sandbox_render_context& ctx)
            {
                ++frameIndex;

                graph.set_input("RenderResolution", vec2u{.x = 16u, .y = 16u});
                // graph.enable_output("FinalRender");

                resourcePool.begin_build();

                resourcePool.begin_graph();
                graph.build(resourcePool);
                resourcePool.end_graph();

                resourcePool.end_build(*ctx.vkContext);

                graph.execute(renderer, resourcePool, *ctx.frameAllocator);
            }

            void update_imgui(const vk::sandbox_update_imgui_context&) {}

            u32 frameIndex{0};
            renderer renderer;
            resource_pool resourcePool;
            render_graph graph;
        };
    }

    TEST(render_graph, render_graph_test)
    {
        sandbox_app<render_graph_test> app;

        app.set_config(sandbox_app_config{
            .vkUseValidationLayers = true,
        });

        const auto cleanup = finally([&app] { app.shutdown(); });
        ASSERT_TRUE(app.init());

        auto* const depthNode = app.graph.find_node<fill_depth_node>();
        auto* const colorNode = app.graph.find_node<fill_color_node>();
        auto* const copyNode = app.graph.find_node<copy_node>();

        ASSERT_TRUE(depthNode);
        ASSERT_TRUE(colorNode);
        ASSERT_TRUE(copyNode);

        ASSERT_NE(depthNode->outDepthBuffer, colorNode->inDepthBuffer);
        ASSERT_NE(depthNode->outDepthBuffer, colorNode->outRenderTarget);
        ASSERT_NE(colorNode->outRenderTarget, copyNode->inSource);
        ASSERT_NE(copyNode->outTarget, copyNode->inSource);

        const auto depthNodeOutDepthTex = app.graph.get_backing_texture_id(depthNode->outDepthBuffer);
        const auto colorNodeInDepthTex = app.graph.get_backing_texture_id(colorNode->inDepthBuffer);
        const auto colorNodeOutRenderTargetTex = app.graph.get_backing_texture_id(colorNode->outRenderTarget);
        const auto copyNodeInSourceTex = app.graph.get_backing_texture_id(copyNode->inSource);
        const auto copyNodeOutTargetTex = app.graph.get_backing_texture_id(copyNode->outTarget);

        ASSERT_EQ(depthNodeOutDepthTex, colorNodeInDepthTex);
        ASSERT_NE(depthNodeOutDepthTex, colorNodeOutRenderTargetTex);
        ASSERT_EQ(colorNodeOutRenderTargetTex, copyNodeInSourceTex);
        ASSERT_NE(copyNodeOutTargetTex, copyNodeInSourceTex);

#if 0
        while (app.run_frame())
        {
        }
#else
        app.run_frame();
#endif
    }
}