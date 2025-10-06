#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_graph_registry.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>

#include <gtest/gtest.h>

namespace oblo
{
    namespace
    {
        class add_f32_node final : public node_interface
        {
        public:
            void on_create(const node_graph_context& g) override
            {
                m_firstOperand = g.add_in_pin({
                    .id = "52a7d901-b9b5-43a3-bcc9-341f5405ec23"_uuid,
                    .name = "A",
                });

                m_secondOperand = g.add_in_pin({
                    .id = "7b5003a9-be09-4a3e-97fb-4d809d65fc57"_uuid,
                    .name = "B",
                });

                m_result = g.add_out_pin({
                    .id = "7c68ac56-3b8d-4e70-ab04-417215e4fb26"_uuid,
                    .name = "Result",
                });
            }

            void on_input_change(const node_graph_context&) override {}

            void fetch_properties_descriptors(dynamic_array<node_property_descriptor>&) const override {}
            void store(data_document&, u32) const override {}
            void load(const data_document&, u32) override {}

            bool generate(const node_graph_context&,
                abstract_syntax_tree&,
                h32<ast_node>,
                const std::span<const h32<ast_node>>,
                dynamic_array<h32<ast_node>>&) const override
            {
                return false;
            }

        private:
            h32<node_graph_in_pin> m_firstOperand{};
            h32<node_graph_in_pin> m_secondOperand{};
            h32<node_graph_out_pin> m_result{};
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

            void on_input_change(const node_graph_context&) override {}

            void fetch_properties_descriptors(dynamic_array<node_property_descriptor>&) const override {}
            void store(data_document&, u32) const override {}

            void load(const data_document& doc, u32 nodeIndex) override
            {
                const auto valueIndex = doc.find_child(nodeIndex, "value"_hsv);
                m_value = doc.read_f32(valueIndex).value_or(0.f);
            }

            bool generate(const node_graph_context&,
                abstract_syntax_tree&,
                h32<ast_node>,
                const std::span<const h32<ast_node>>,
                dynamic_array<h32<ast_node>>&) const override
            {
                return false;
            }

        private:
            f32 m_value{};
        };

        constexpr uuid g_AddFloatsNodeId = "f46ed757-59b5-414c-bc62-c7935c254904"_uuid;
        constexpr uuid g_FloatConstantNodeId = "5349df64-09e7-465a-aea6-c57b16fd7490"_uuid;

        node_descriptor make_add_floats_node()
        {
            return {
                .id = "f46ed757-59b5-414c-bc62-c7935c254904"_uuid,
                .name = "Add Floats",
                .instantiate = [](const any&) -> unique_ptr<node_interface> { return allocate_unique<add_f32_node>(); },
            };
        }

        node_descriptor make_float_constant_node()
        {
            return {
                .id = "5349df64-09e7-465a-aea6-c57b16fd7490"_uuid,
                .name = "Float Constant",
                .instantiate = [](const any&) -> unique_ptr<node_interface>
                { return allocate_unique<f32_constant_node>(); },
            };
        }
    }

    TEST(node_graph, add_floats)
    {
        node_graph_registry registry;

        ASSERT_TRUE(registry.register_node(make_add_floats_node()));
        ASSERT_TRUE(registry.register_node(make_float_constant_node()));

        node_graph g;
        g.init(registry);

        const h32 addNodeHandle = g.add_node(g_AddFloatsNodeId);
        ASSERT_TRUE(addNodeHandle);

        const h32 f32ConstA = g.add_node(g_FloatConstantNodeId);
        ASSERT_TRUE(f32ConstA);

        const h32 f32ConstB = g.add_node(g_FloatConstantNodeId);
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

        {
            data_document docA;
            docA.init();
            docA.child_value(docA.get_root(), "value"_hsv, property_value_wrapper{16.f});
            g.load(f32ConstA, docA, docA.get_root());
        }

        {
            data_document docB;
            docB.init();
            docB.child_value(docB.get_root(), "value"_hsv, property_value_wrapper{26.f});
            g.load(f32ConstB, docB, docB.get_root());
        }
    }
}
