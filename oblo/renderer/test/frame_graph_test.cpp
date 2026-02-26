#include <oblo/app/graphics_app.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/graph/dot.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/renderer/graph/frame_graph.hpp>
#include <oblo/renderer/graph/frame_graph_context.hpp>
#include <oblo/renderer/graph/frame_graph_registry.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/renderer.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <gtest/gtest.h>

namespace oblo::test
{
    namespace
    {
        struct fgt_cpu_data
        {
            dynamic_array<std::string>* executionLog;
        };

        struct fgt_gbuffer_pass
        {
            pin::data<fgt_cpu_data> inCpuData;
            pin::texture outGBuffer;
            pin::texture outDepth;

            void build(const frame_graph_build_context& ctx)
            {
                auto& cpuData = ctx.access(inCpuData);
                cpuData.executionLog->push_back("fgt_gbuffer_pass::build");
            }

            void execute(const frame_graph_execute_context& ctx)
            {
                auto& cpuData = ctx.access(inCpuData);
                cpuData.executionLog->push_back("fgt_gbuffer_pass::execute");
            }
        };

        struct fgt_lighting_pass
        {
            pin::data<fgt_cpu_data> inCpuData;
            pin::data<u32> inShadowMapAtlasId;
            pin::texture inShadowMap;
            pin::texture inGBuffer;
            pin::texture outLit;

            void build(const frame_graph_build_context& ctx)
            {
                auto& cpuData = ctx.access(inCpuData);
                cpuData.executionLog->push_back("fgt_lighting_pass::build");
            }

            void execute(const frame_graph_execute_context& ctx)
            {
                auto& cpuData = ctx.access(inCpuData);
                cpuData.executionLog->push_back("fgt_lighting_pass::execute");

                auto& id = ctx.access(inShadowMapAtlasId);
                ASSERT_TRUE(id == 42);
            }
        };

        struct fgt_shadow_pass
        {
            pin::data<fgt_cpu_data> inCpuData;
            pin::texture outShadowMap;
            pin::data<u32> outShadowMapAtlasId;

            void build(const frame_graph_build_context& ctx)
            {
                auto& cpuData = ctx.access(inCpuData);
                cpuData.executionLog->push_back("fgt_shadow_pass::build");
            }

            void execute(const frame_graph_execute_context& ctx)
            {
                auto& cpuData = ctx.access(inCpuData);
                cpuData.executionLog->push_back("fgt_shadow_pass::execute");

                auto& id = ctx.access(outShadowMapAtlasId);
                id = 42;
            }
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

            tmpl.connect(gbufferNode, &fgt_gbuffer_pass::inCpuData, lightingNode, &fgt_lighting_pass::inCpuData);
            tmpl.connect(gbufferNode, &fgt_gbuffer_pass::outGBuffer, lightingNode, &fgt_lighting_pass::inGBuffer);
            tmpl.make_input(gbufferNode, &fgt_gbuffer_pass::inCpuData, "in_CpuData");
            tmpl.make_input(lightingNode, &fgt_lighting_pass::inShadowMap, "in_ShadowMap");
            tmpl.make_input(lightingNode, &fgt_lighting_pass::inShadowMapAtlasId, "in_ShadowMapAtlasId");
            tmpl.make_output(lightingNode, &fgt_lighting_pass::outLit, "out_Lit");

            return tmpl;
        }

        frame_graph_template fgt_create_shadow_map(const frame_graph_registry& registry)
        {
            frame_graph_template tmpl;

            tmpl.init(registry);

            const auto shadowPassNode = tmpl.add_node<fgt_shadow_pass>();

            tmpl.make_input(shadowPassNode, &fgt_shadow_pass::inCpuData, "in_CpuData");
            tmpl.make_output(shadowPassNode, &fgt_shadow_pass::outShadowMap, "out_ShadowMap");
            tmpl.make_output(shadowPassNode, &fgt_shadow_pass::outShadowMapAtlasId, "out_ShadowMapAtlasId");

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

        ASSERT_EQ(inputs.size(), 3);
        ASSERT_EQ(outputs.size(), 1);

        ASSERT_EQ(mainViewTemplate.get_name(inputs[0]), "in_CpuData");
        ASSERT_EQ(mainViewTemplate.get_name(inputs[1]), "in_ShadowMap");
        ASSERT_EQ(mainViewTemplate.get_name(inputs[2]), "in_ShadowMapAtlasId");

        ASSERT_EQ(mainViewTemplate.get_name(outputs[0]), "out_Lit");

        dynamic_array<frame_graph_template::vertex_handle> sorted;
        ASSERT_TRUE(topological_sort(g, sorted));

        if constexpr (false)
        {
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
            std::puts(str.c_str());
        }

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

    TEST(frame_graph, frame_graph_mock_shadow)
    {
        module_manager mm;
        auto* vkEngine = mm.load<vk::vulkan_engine_module>();

        ASSERT_TRUE(mm.finalize());

        graphics_app app;

        ASSERT_TRUE(app.init({}));

        while (!app.acquire_images())
            ;

        dynamic_array<std::string> executionLog;

        const auto registry = fgt_create_registry();

        const auto mainViewTemplate = fgt_create_main_view(registry);

        frame_graph frameGraph;
        frameGraph.init(vkEngine->get_gpu_instance());

        const auto mainView = frameGraph.instantiate(mainViewTemplate);
        ASSERT_TRUE(mainView);

        ASSERT_TRUE(frameGraph.set_input(mainView, "in_CpuData", fgt_cpu_data{&executionLog}));

        const auto shadowMapTemplate = fgt_create_shadow_map(registry);

        const auto shadowMapping = frameGraph.instantiate(shadowMapTemplate);
        ASSERT_TRUE(shadowMapping);

        ASSERT_TRUE(frameGraph.set_input(shadowMapping, "in_CpuData", fgt_cpu_data{&executionLog}));

        ASSERT_TRUE(frameGraph.connect(shadowMapping, "out_ShadowMap", mainView, "in_ShadowMap"));
        ASSERT_TRUE(frameGraph.connect(shadowMapping, "out_ShadowMapAtlasId", mainView, "in_ShadowMapAtlasId"));

        auto& renderer = vkEngine->get_renderer();

        const frame_graph_build_args buildArgs{
            .rendererPlatform = renderer.get_renderer_platform(),
            .gpu = renderer.get_gpu_instance(),
            .stagingBuffer = renderer.get_staging_buffer(),
        };

        frameGraph.build(buildArgs);

        // Order between gbuffer and shadow is not determined, but lighting has to run last
        ASSERT_EQ(executionLog.size(), 3);
        ASSERT_TRUE(executionLog[1] != executionLog[0]);
        ASSERT_TRUE(executionLog[0] == "fgt_gbuffer_pass::build" || executionLog[0] == "fgt_shadow_pass::build");
        ASSERT_TRUE(executionLog[1] == "fgt_gbuffer_pass::build" || executionLog[1] == "fgt_shadow_pass::build");
        ASSERT_TRUE(executionLog[2] == "fgt_lighting_pass::build");

        executionLog.clear();

        const frame_graph_execute_args executeArgs{
            .rendererPlatform = renderer.get_renderer_platform(),
            .gpu = renderer.get_gpu_instance(),
            .commandBuffer = renderer.get_active_command_buffer(),
            .stagingBuffer = renderer.get_staging_buffer(),
        };

        frameGraph.execute(executeArgs);

        ASSERT_EQ(executionLog.size(), 3);
        ASSERT_TRUE(executionLog[1] != executionLog[0]);
        ASSERT_TRUE(executionLog[0] == "fgt_gbuffer_pass::execute" || executionLog[0] == "fgt_shadow_pass::execute");
        ASSERT_TRUE(executionLog[1] == "fgt_gbuffer_pass::execute" || executionLog[1] == "fgt_shadow_pass::execute");
        ASSERT_TRUE(executionLog[2] == "fgt_lighting_pass::execute");
    }
}