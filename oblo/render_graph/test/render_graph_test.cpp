#include <gtest/gtest.h>

#include <oblo/render_graph/render_graph.hpp>
#include <oblo/render_graph/render_graph_builder.hpp>
#include <oblo/render_graph/render_graph_seq_executor.hpp>

namespace oblo
{
    namespace
    {
        struct mock_context
        {
            int numExecuted;
        };

        struct buffer_ref
        {
            u32 ref;
        };

        struct image_ref
        {
            u32 ref;
        };

        struct mock_deferred_gbuffer_node
        {
            render_node_in<buffer_ref, "camera"> camera;
            render_node_out<image_ref, "gbuffer"> gbuffer;

            u32 foo{42};

            void execute(mock_context* context)
            {
                ASSERT_TRUE(context);
                ASSERT_TRUE(context->numExecuted == 0);
                ++context->numExecuted;

                ASSERT_EQ(foo, 42);
            }

            void execute(void*)
            {
                ASSERT_FALSE(true);
            }
        };

        struct mock_deferred_lighting_node
        {
            render_node_in<buffer_ref, "camera"> camera;
            render_node_in<buffer_ref, "lights"> lights;
            render_node_in<image_ref, "gbuffer"> gbuffer;
            render_node_out<image_ref, "lit"> lit;

            void execute(mock_context* context)
            {
                ASSERT_TRUE(context);
                ASSERT_TRUE(context->numExecuted == 1);
                ++context->numExecuted;
            }

            void execute(void*)
            {
                ASSERT_FALSE(true);
            }
        };
    }

    TEST(render_graph, mock_deferred_graph)
    {
        render_graph graph;
        render_graph_seq_executor executor;

        const auto ec = render_graph_builder<mock_context>{}
                            .add_node<mock_deferred_gbuffer_node>()
                            .add_node<mock_deferred_lighting_node>()
                            .add_edge(&mock_deferred_gbuffer_node::gbuffer, &mock_deferred_lighting_node::gbuffer)
                            .add_broadcast_input<buffer_ref>("camera")
                            .add_broadcast_input<buffer_ref>("lights")
                            .build(graph, executor);

        ASSERT_FALSE(ec);

        auto* gbufferNode = graph.find_node<mock_deferred_gbuffer_node>();
        auto* lightingNode = graph.find_node<mock_deferred_lighting_node>();

        ASSERT_TRUE(gbufferNode);
        ASSERT_TRUE(lightingNode);

        ASSERT_EQ(gbufferNode->foo, 42);

        ASSERT_NE(gbufferNode->camera.data, nullptr);
        ASSERT_NE(lightingNode->camera.data, nullptr);
        ASSERT_EQ(gbufferNode->camera.data, lightingNode->camera.data);

        ASSERT_NE(lightingNode->lit.data, nullptr);

        ASSERT_NE(gbufferNode->gbuffer.data, nullptr);
        ASSERT_NE(lightingNode->gbuffer.data, nullptr);
        ASSERT_EQ(gbufferNode->gbuffer.data, lightingNode->gbuffer.data);

        ASSERT_FALSE(graph.find_input<image_ref>("camera"));

        auto* const camera = graph.find_input<buffer_ref>("camera");
        ASSERT_TRUE(camera);

        ASSERT_EQ(camera, gbufferNode->camera.data);
        ASSERT_EQ(camera, lightingNode->camera.data);

        auto* const lights = graph.find_input<buffer_ref>("lights");
        ASSERT_TRUE(lights);
        ASSERT_EQ(lights, lightingNode->lights.data);

        mock_context context{.numExecuted = 0};
        executor.execute(&context);
        ASSERT_TRUE(context.numExecuted == 2);
    }
}