#include <oblo/core/finally.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/graph/dot.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/sandbox/sandbox_app.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/texture.hpp>

#include <gtest/gtest.h>

namespace oblo::vk::test
{
    namespace
    {
        struct fgt_cpu_data
        {
            dynamic_array<std::string>* executionLog;
        };

        struct fgt_gbuffer_pass
        {
            data<fgt_cpu_data> inCpuData;
            resource<texture> outGBuffer;
            resource<texture> outDepth;

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
            data<fgt_cpu_data> inCpuData;
            data<u32> inShadowMapAtlasId;
            resource<texture> inShadowMap;
            resource<texture> inGBuffer;
            resource<texture> outLit;

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
            data<fgt_cpu_data> inCpuData;
            resource<texture> outShadowMap;
            data<u32> outShadowMapAtlasId;

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

        struct test_wrapper
        {
            bool init(const sandbox_init_context& ctx)
            {
                frameAllocator.init(1u << 26);

                entities.init(&typeRegistry);

                if (!renderer.init({
                        .vkContext = *ctx.vkContext,
                        .frameAllocator = frameAllocator,
                        .entities = entities,
                    }))
                {
                    return false;
                }

                return true;
            }

            void shutdown(const sandbox_shutdown_context&)
            {
                renderer.shutdown();
                frameAllocator.shutdown();
            }

            void update(const sandbox_render_context& ctx)
            {
                updateCb(ctx);
            }

            void update_imgui(const sandbox_update_imgui_context&) {}

            frame_allocator frameAllocator;

            renderer renderer;

            ecs::type_registry typeRegistry;
            ecs::entity_registry entities;

            std::function<void(const sandbox_render_context&)> updateCb;
        };
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

    TEST(frame_graph, frame_graph_mock_shadow)
    {
        dynamic_array<std::string> executionLog;

        const auto registry = fgt_create_registry();

        const auto mainViewTemplate = fgt_create_main_view(registry);

        sandbox_app<test_wrapper> app;

        app.updateCb = [&](const sandbox_render_context& ctx)
        {
            frame_graph frameGraph;
            frameGraph.init(*ctx.vkContext);

            const auto mainView = frameGraph.instantiate(mainViewTemplate);
            ASSERT_TRUE(mainView);

            ASSERT_TRUE(frameGraph.set_input(mainView, "in_CpuData", fgt_cpu_data{&executionLog}));

            const auto shadowMapTemplate = fgt_create_shadow_map(registry);

            const auto shadowMapping = frameGraph.instantiate(shadowMapTemplate);
            ASSERT_TRUE(shadowMapping);

            ASSERT_TRUE(frameGraph.set_input(shadowMapping, "in_CpuData", fgt_cpu_data{&executionLog}));

            ASSERT_TRUE(frameGraph.connect(shadowMapping, "out_ShadowMap", mainView, "in_ShadowMap"));
            ASSERT_TRUE(frameGraph.connect(shadowMapping, "out_ShadowMapAtlasId", mainView, "in_ShadowMapAtlasId"));

            frameGraph.build(app.renderer);

            // Order between gbuffer and shadow is not determined, but lighting has to run last
            ASSERT_EQ(executionLog.size(), 3);
            ASSERT_TRUE(executionLog[1] != executionLog[0]);
            ASSERT_TRUE(executionLog[0] == "fgt_gbuffer_pass::build" || executionLog[0] == "fgt_shadow_pass::build");
            ASSERT_TRUE(executionLog[1] == "fgt_gbuffer_pass::build" || executionLog[1] == "fgt_shadow_pass::build");
            ASSERT_TRUE(executionLog[2] == "fgt_lighting_pass::build");

            executionLog.clear();

            frameGraph.execute(app.renderer);

            ASSERT_EQ(executionLog.size(), 3);
            ASSERT_TRUE(executionLog[1] != executionLog[0]);
            ASSERT_TRUE(
                executionLog[0] == "fgt_gbuffer_pass::execute" || executionLog[0] == "fgt_shadow_pass::execute");
            ASSERT_TRUE(
                executionLog[1] == "fgt_gbuffer_pass::execute" || executionLog[1] == "fgt_shadow_pass::execute");
            ASSERT_TRUE(executionLog[2] == "fgt_lighting_pass::execute");
        };

        app.set_config(sandbox_app_config{
            .vkUseValidationLayers = true,
        });

        ASSERT_TRUE(app.init());
        const auto cleanup = finally([&] { app.shutdown(); });

        app.run_frame();
    }
}