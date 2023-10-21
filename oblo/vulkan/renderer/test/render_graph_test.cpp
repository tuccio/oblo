#include <gtest/gtest.h>

#include <oblo/core/array_size.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/sandbox/sandbox_app.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
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

            h32<render_pass> renderPass{};

            void build(runtime_builder& builder)
            {
                const auto resolution = builder.access(inResolution);

                builder.create(outDepthBuffer,
                               {
                                   .width = resolution.x,
                                   .height = resolution.y,
                                   .format = VK_FORMAT_D16_UNORM,
                                   .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                   .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                               },
                               resource_usage::depth_stencil_write);
            }

            void execute(const runtime_context& context)
            {
                auto& renderPassManager = context.get_render_pass_manager();

                if (!renderPass)
                {
                    renderPass = renderPassManager.register_render_pass({
                        .name = "fill_depth_node",
                        .stages =
                            {
                                {
                                    .stage = pipeline_stages::vertex,
                                    .shaderSourcePath = OBLO_TEST_RESOURCES "/shaders/basic.vert",
                                },
                            },
                    });
                }

                const VkCommandBuffer commandBuffer = context.get_command_buffer();

                const auto depthBuffer = context.access(outDepthBuffer);

                auto& frameAllocator = context.get_frame_allocator();

                const auto pipeline =
                    renderPassManager.get_or_create_pipeline(frameAllocator,
                                                             renderPass,
                                                             {
                                                                 .renderTargets =
                                                                     {
                                                                         .depthFormat = depthBuffer.initializer.format,
                                                                     },
                                                                 .depthStencilState =
                                                                     {
                                                                         .depthTestEnable = true,
                                                                         .depthWriteEnable = true,
                                                                         .depthCompareOp = VK_COMPARE_OP_ALWAYS,
                                                                     },
                                                                 .rasterizationState =
                                                                     {
                                                                         .polygonMode = VK_POLYGON_MODE_FILL,
                                                                         .cullMode = VK_CULL_MODE_NONE,
                                                                         .lineWidth = 1.f,
                                                                     },
                                                             });

                render_pass_context renderPassContext{
                    .commandBuffer = commandBuffer,
                    .pipeline = pipeline,
                    .frameAllocator = frameAllocator,
                };

                const VkRenderingAttachmentInfo depthAttachment{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = depthBuffer.view,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                };

                const auto [renderWidth, renderHeight, _] = depthBuffer.initializer.extent;

                const VkRenderingInfo renderInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea = {.extent{.width = renderWidth, .height = renderHeight}},
                    .layerCount = 1,
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

                vkCmdDrawIndexed(commandBuffer, meshTable.index_count(), 1, 0, 0, 0);

                renderPassManager.end_rendering(renderPassContext);
            }
        };

        struct fill_color_node
        {
            resource<texture> outRenderTarget;
            resource<texture> inDepthBuffer;
            data<vec2u> inResolution;

            h32<render_pass> renderPass{};

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

                builder.acquire(inDepthBuffer, resource_usage::depth_stencil_read);
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
                        .depthStencilState =
                            {
                                .depthTestEnable = true,
                                .depthWriteEnable = false,
                                .depthCompareOp = VK_COMPARE_OP_EQUAL,
                            },
                        .rasterizationState =
                            {
                                .polygonMode = VK_POLYGON_MODE_FILL,
                                .cullMode = VK_CULL_MODE_NONE,
                                .lineWidth = 1.f,
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
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
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

                vkCmdDrawIndexed(commandBuffer, meshTable.index_count(), 1, 0, 0, 0);

                renderPassManager.end_rendering(renderPassContext);
            }
        };

        struct download_node
        {
            data<VkBuffer> inDownloadRenderTarget;
            data<VkBuffer> inDownloadDepth;

            resource<texture> inRenderTarget;
            resource<texture> inDetphBuffer;

            void build(runtime_builder& builder)
            {
                builder.acquire(inRenderTarget, resource_usage::transfer_source);
                builder.acquire(inDetphBuffer, resource_usage::transfer_source);
            }

            void execute(const runtime_context& context)
            {
                const auto cb = context.get_command_buffer();

                const auto srcRenderTarget = context.access(inRenderTarget);
                const auto srcDepthBuffer = context.access(inDetphBuffer);

                auto* const dstRenderTarget = context.access(inDownloadRenderTarget);
                auto* const dstDepthBuffer = context.access(inDownloadDepth);

                const VkBufferImageCopy renderTargetRegion{
                    .imageSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .layerCount = 1,
                        },
                    .imageExtent = srcRenderTarget.initializer.extent,
                };

                vkCmdCopyImageToBuffer(cb,
                                       srcRenderTarget.image,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       *dstRenderTarget,
                                       1,
                                       &renderTargetRegion);

                const VkBufferImageCopy depthBufferRegion{
                    .imageSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                            .layerCount = 1,
                        },
                    .imageExtent = srcDepthBuffer.initializer.extent,
                };

                vkCmdCopyImageToBuffer(cb,
                                       srcDepthBuffer.image,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       *dstDepthBuffer,
                                       1,
                                       &depthBufferRegion);
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
                .numVertices = 4,
                .numIndices = 6,
            };

            if (!meshes.allocate_meshes({&mesh, 1}))
            {
                return;
            }

            const h32<string> columnSubset[] = {position};
            buffer buffers[array_size(columnSubset)]{};
            buffer indexBuffer{};

            meshes.fetch_buffers(resourceManager, columnSubset, buffers, &indexBuffer);

            constexpr vec3 positions[] = {
                {-1.f, -1.f, 1.f},
                {-1.f, 1.f, 1.f},
                {1.f, 1.f, 1.f},
                {1.f, -1.f, 1.f},
            };

            constexpr u32 indices[] = {0, 1, 2, 0, 2, 3};

            auto& stagingBuffer = renderer.get_staging_buffer();

            stagingBuffer.upload(std::as_bytes(std::span{positions}), buffers[0].buffer, buffers[0].offset);
            stagingBuffer.upload(std::as_bytes(std::span{indices}), indexBuffer.buffer, indexBuffer.offset);

            stagingBuffer.flush();
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
                                   .add_node<download_node>()
                                   .add_input<vec2u>("RenderResolution")
                                   .add_input<VkBuffer>("DepthDownload")
                                   .add_input<VkBuffer>("RenderTargetDownload")
                                   .add_output<h32<texture>>("FinalRender")
                                   .add_output<h32<texture>>("DepthBuffer")
                                   .connect_input("RenderResolution", &fill_depth_node::inResolution)
                                   .connect_input("RenderResolution", &fill_color_node::inResolution)
                                   .connect(&fill_depth_node::outDepthBuffer, &fill_color_node::inDepthBuffer)
                                   .connect(&fill_color_node::inDepthBuffer, &download_node::inDetphBuffer)
                                   .connect(&fill_color_node::outRenderTarget, &download_node::inRenderTarget)
                                   .connect_output(&download_node::inRenderTarget, "FinalRender")
                                   .connect_output(&download_node::inDetphBuffer, "DepthBuffer")
                                   .connect_input("DepthDownload", &download_node::inDownloadDepth)
                                   .connect_input("RenderTargetDownload", &download_node::inDownloadRenderTarget)
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

                constexpr vec2u resolution{.x = 16u, .y = 16u};

                graph.set_input("RenderResolution", resolution);
                graph.set_input("DepthDownload", depthImageDownload.buffer);
                graph.set_input("RenderTargetDownload", renderTargetDownload.buffer);
                // graph.enable_output("FinalRender");

                resourcePool.begin_build();

                resourcePool.begin_graph();
                graph.build(resourcePool);
                resourcePool.end_graph();

                resourcePool.end_build(*ctx.vkContext);

                graph.execute(renderer, resourcePool, *ctx.frameAllocator);
            }

            void update_imgui(const vk::sandbox_update_imgui_context&) {}

            static constexpr vec2u resolution{.x = 16u, .y = 16u};

            allocated_buffer depthImageDownload{};
            allocated_buffer renderTargetDownload{};

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
        auto* const downloadNode = app.graph.find_node<download_node>();

        ASSERT_TRUE(depthNode);
        ASSERT_TRUE(colorNode);
        ASSERT_TRUE(downloadNode);

        ASSERT_NE(depthNode->outDepthBuffer, colorNode->inDepthBuffer);
        ASSERT_NE(depthNode->outDepthBuffer, colorNode->outRenderTarget);
        ASSERT_NE(colorNode->outRenderTarget, downloadNode->inRenderTarget);
        ASSERT_NE(depthNode->outDepthBuffer, downloadNode->inDetphBuffer);

        const auto depthNodeOutDepthTex = app.graph.get_backing_texture_id(depthNode->outDepthBuffer);
        const auto colorNodeInDepthTex = app.graph.get_backing_texture_id(colorNode->inDepthBuffer);
        const auto colorNodeOutRenderTargetTex = app.graph.get_backing_texture_id(colorNode->outRenderTarget);
        const auto downloadDepthBufferTex = app.graph.get_backing_texture_id(downloadNode->inDetphBuffer);
        const auto downloadRenderTargetTex = app.graph.get_backing_texture_id(downloadNode->inRenderTarget);

        ASSERT_EQ(depthNodeOutDepthTex, colorNodeInDepthTex);
        ASSERT_NE(depthNodeOutDepthTex, colorNodeOutRenderTargetTex);
        ASSERT_EQ(colorNodeOutRenderTargetTex, downloadRenderTargetTex);
        ASSERT_EQ(depthNodeOutDepthTex, downloadDepthBufferTex);

        auto& allocator = app.renderer.get_allocator();

        OBLO_VK_PANIC(allocator.create_buffer(
            {
                .size = sizeof(u16) * app.resolution.x * app.resolution.y,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .memoryUsage = memory_usage::gpu_to_cpu,
            },
            &app.depthImageDownload));

        OBLO_VK_PANIC(allocator.create_buffer(
            {
                .size = sizeof(u32) * app.resolution.x * app.resolution.y,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .memoryUsage = memory_usage::gpu_to_cpu,
            },
            &app.renderTargetDownload));

#if 0
        while (app.run_frame())
        {
        }
#else
        app.run_frame();
#endif

        auto& engine = app.renderer.get_engine();
        vkDeviceWaitIdle(engine.get_device());

        void* depthBufferData;
        void* renderTargetData;

        OBLO_VK_PANIC(allocator.map(app.depthImageDownload.allocation, &depthBufferData));
        OBLO_VK_PANIC(allocator.map(app.renderTargetDownload.allocation, &renderTargetData));

        const auto destroyBuffers = finally(
            [&allocator, &app]
            {
                allocator.destroy(app.depthImageDownload);
                allocator.destroy(app.renderTargetDownload);
            });

        constexpr u32 N{app.resolution.x * app.resolution.y};
        auto* const depthU16 = start_lifetime_as_array<u16>(depthBufferData, N);
        auto* const colorU32 = start_lifetime_as_array<u32>(renderTargetData, N);

        for (u32 i = 0; i < N; ++i)
        {
            ASSERT_EQ(depthU16[i], 0xffff);
            ASSERT_EQ(colorU32[i], 0xff0000ff);
        }
    }
}