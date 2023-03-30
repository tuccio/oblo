#include <gtest/gtest.h>

#include <oblo/render_graph/render_graph_builder.hpp>

namespace oblo
{
    namespace
    {
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

            void execute() {}
        };

        struct mock_deferred_lighting_node
        {
            render_node_in<buffer_ref, "camera"> camera;
            render_node_in<buffer_ref, "lights"> lights;
            render_node_in<image_ref, "gbuffer"> gbuffer;
            render_node_out<image_ref, "lit"> lit;

            void execute() {}
        };
    }

    TEST(render_graph, mock_deferred_graph_build)
    {
        auto graph = render_graph_builder{}
                         .add_node<mock_deferred_gbuffer_node>()
                         .add_node<mock_deferred_lighting_node>()
                         .add_edge(&mock_deferred_gbuffer_node::gbuffer, &mock_deferred_lighting_node::gbuffer)
                         .add_broadcast_input<buffer_ref>("camera")
                         .build();

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
    }
}