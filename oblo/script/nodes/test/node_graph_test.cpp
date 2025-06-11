#include <oblo/script/nodes/node_descriptor.hpp>
#include <oblo/script/nodes/node_graph.hpp>
#include <oblo/script/nodes/node_graph_registry.hpp>

#include <gtest/gtest.h>

namespace oblo::script
{
    namespace
    {
        class add_f32_node final : public node_interface
        {
        public:
            void on_create(const node_graph_context& g) override
            {
                g.add_in_pin({
                    .id = "52a7d901-b9b5-43a3-bcc9-341f5405ec23"_uuid,
                    .name = "A",
                });

                g.add_in_pin({
                    .id = "7b5003a9-be09-4a3e-97fb-4d809d65fc57"_uuid,
                    .name = "B",
                });

                g.add_out_pin({
                    .id = "7c68ac56-3b8d-4e70-ab04-417215e4fb26"_uuid,
                    .name = "Result",
                });
            }

            void on_change(const node_graph_context&) override
            {
                // TODO: Deduce types?
            }

            void store(data_document&, u32) override {}
            void load(const data_document&, u32) const override {}
        };

        class f32_constant_node final : public node_interface
        {
        public:
            void on_create(const node_graph_context& g) override
            {
                g.add_out_pin({
                    .id = "4d702e17-4ec3-4902-b693-12fa1d67727c"_uuid,
                    .name = "Value",
                });
            }

            void on_change(const node_graph_context&) override {}

            void store(data_document&, u32) override
            {
                // TODO: Store float
            }

            void load(const data_document&, u32) const override
            {
                // TODO: Load float
            }

        private:
            f32 m_value{};
        };

        const node_descriptor g_AddFloatsNode{
            .id = "f46ed757-59b5-414c-bc62-c7935c254904"_uuid,
            .name = "Add Floats",
            .instantiate = []() -> unique_ptr<node_interface> { return allocate_unique<add_f32_node>(); },
        };

        const node_descriptor g_FloatConstantNode{
            .id = "5349df64-09e7-465a-aea6-c57b16fd7490"_uuid,
            .name = "Float Constant",
            .instantiate = []() -> unique_ptr<node_interface> { return allocate_unique<f32_constant_node>(); },
        };
    }

    TEST(node_graph, add_floats)
    {
        node_graph_registry registry;

        registry.register_node(g_AddFloatsNode);
        registry.register_node(g_FloatConstantNode);

        node_graph g;
        g.init(registry);

        const h32 addNodeHandle = g.add_node(g_AddFloatsNode.id);
        ASSERT_TRUE(addNodeHandle);

        const h32 f32ConstA = g.add_node(g_FloatConstantNode.id);
        ASSERT_TRUE(f32ConstA);

        const h32 f32ConstB = g.add_node(g_FloatConstantNode.id);
        ASSERT_TRUE(f32ConstB);

        dynamic_array<h32<node_graph_out_pin>> outA, outB, outAdd;
        dynamic_array<h32<node_graph_in_pin>> inAdd;

        g.fetch_out_pins(addNodeHandle, outAdd);
        g.fetch_in_pins(addNodeHandle, inAdd);

        g.fetch_out_pins(f32ConstA, outA);
        g.fetch_out_pins(f32ConstB, outB);

        ASSERT_EQ(outAdd.size(), 1);
        ASSERT_EQ(inAdd.size(), 2);
        ASSERT_EQ(outA.size(), 1);
        ASSERT_EQ(outB.size(), 1);

        ASSERT_TRUE(outAdd[0]);
        ASSERT_TRUE(inAdd[0]);
        ASSERT_TRUE(inAdd[1]);
        ASSERT_TRUE(outA[0]);
        ASSERT_TRUE(outB[0]);

        g.connect(outA[0], inAdd[0]);
        g.connect(outB[0], inAdd[1]);
    }
}
