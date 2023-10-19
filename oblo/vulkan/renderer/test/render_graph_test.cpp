#include <gtest/gtest.h>

#include <oblo/core/finally.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/sandbox/sandbox_app.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/graph/topology_builder.hpp>
#include <oblo/vulkan/renderer.hpp>

namespace oblo::vk::test
{
    namespace
    {
        struct fill_depth_node
        {
            resource<texture> outDepthBuffer;
            data<vec2u> inResolution;
            data<f32> inDepth;

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
            data<vec3> inColor;

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
            }

            void execute(const runtime_context& context)
            {
                const texture renderTarget = context.access(outRenderTarget);
                const texture depthBuffer = context.access(inDepthBuffer);

                (void) renderTarget;
                (void) depthBuffer;
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

                graph.execute(renderer, resourcePool);
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

        ASSERT_EQ(app.graph.get_backing_texture_id(depthNode->outDepthBuffer),
                  app.graph.get_backing_texture_id(colorNode->inDepthBuffer));
        ASSERT_NE(app.graph.get_backing_texture_id(depthNode->outDepthBuffer),
                  app.graph.get_backing_texture_id(colorNode->outRenderTarget));
        ASSERT_EQ(app.graph.get_backing_texture_id(colorNode->outRenderTarget),
                  app.graph.get_backing_texture_id(copyNode->inSource));
        ASSERT_NE(app.graph.get_backing_texture_id(copyNode->outTarget),
                  app.graph.get_backing_texture_id(copyNode->inSource));

        app.run_frame();
    }
}