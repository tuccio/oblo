#include <oblo/core/graph/dot.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/texture.hpp>

#include <gtest/gtest.h>

namespace oblo::vk::test
{
    namespace
    {
        struct fgt_camera_data
        {
        };

        struct fgt_light_data
        {
        };

        struct fgt_gbuffer_pass
        {
            data<fgt_camera_data> inCameraData;
            resource<texture> outGBuffer;
            resource<texture> outDepth;
        };

        struct fgt_lighting_pass
        {
            data<fgt_light_data> inLightData;
            resource<texture> inGBuffer;
            resource<texture> outLit;
        };

        struct fgt_shadow_pass
        {
            data<fgt_light_data> inLightData;
            data<u32> inLightIndex;
            resource<texture> outShadowMap;
        };

        frame_graph_registry fgt_create_registry()
        {
            frame_graph_registry registry;

            registry.register_node<fgt_gbuffer_pass>();
            registry.register_node<fgt_lighting_pass>();
            registry.register_node<fgt_shadow_pass>();

            return registry;
        }

        frame_graph_template fgt_create_main_view(const frame_graph_registry& registry)
        {
            frame_graph_template tmpl;

            tmpl.init(registry);

            const auto gbufferNode = tmpl.add_node<fgt_gbuffer_pass>();
            const auto lightingNode = tmpl.add_node<fgt_lighting_pass>();

            tmpl.connect(gbufferNode, &fgt_gbuffer_pass::outGBuffer, lightingNode, &fgt_lighting_pass::inGBuffer);
            tmpl.make_input(gbufferNode, &fgt_gbuffer_pass::inCameraData, "in_Camera");
            tmpl.make_input(lightingNode, &fgt_lighting_pass::inLightData, "in_LightData");
            tmpl.make_output(lightingNode, &fgt_lighting_pass::outLit, "out_Lit");

            return tmpl;
        }
    }

    TEST(frame_graph_template, frame_graph_template_main_view)
    {
        const auto registry = fgt_create_registry();

        const auto mainViewTemplate = fgt_create_main_view(registry);

        const auto& g = mainViewTemplate.get_graph();

        const std::span inputs = mainViewTemplate.get_inputs();
        const std::span outputs = mainViewTemplate.get_outputs();

        ASSERT_EQ(inputs.size(), 2);
        ASSERT_EQ(outputs.size(), 1);

        ASSERT_EQ(mainViewTemplate.get_name(inputs[0]), "in_Camera");
        ASSERT_EQ(mainViewTemplate.get_name(inputs[1]), "in_LightData");

        ASSERT_EQ(mainViewTemplate.get_name(outputs[0]), "out_Lit");

        dynamic_array<frame_graph_template::vertex_handle> sorted;
        ASSERT_TRUE(topological_sort(g, sorted));

#if 0 // Useful to see the result
        std::stringstream ss;

        write_graphviz_dot(ss,
            g,
            [&g](auto v) -> std::string
            {
                const auto& d = g[v];

                if (!d.name.empty())
                {
                    return d.name;
                }

                return std::to_string(d.pinMemberOffset);
            });

        const auto str = ss.str();
#endif

        dynamic_array<frame_graph_template::vertex_handle> sortedNodes;

        for (const auto v : sorted)
        {
            const auto& data = g[v];

            if (data.kind == frame_graph_vertex_kind::node)
            {
                sortedNodes.emplace_back(v);
            }
        }

        ASSERT_EQ(sortedNodes.size(), 2);

        ASSERT_EQ(g[sortedNodes[0]].nodeId, registry.get_uuid<fgt_lighting_pass>());
        ASSERT_EQ(g[sortedNodes[1]].nodeId, registry.get_uuid<fgt_gbuffer_pass>());
    }
}