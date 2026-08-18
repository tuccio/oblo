#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_graph_registry.hpp>
#include <oblo/nodes/node_primitive_type.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>

#include <gtest/gtest.h>

namespace oblo
{
    namespace
    {
        constexpr uuid g_ExecutionTypeId = "7f677f4e-c888-407f-a8c7-8d509d2766df"_uuid;
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

        class event_node final : public node_interface
        {
        public:
            void on_create(const node_graph_context& g) override
            {
                const h32 outPin = g.add_out_pin({
                    .id = "8bd4d44c-93bb-4b3c-8ea3-300000000001"_uuid,
                    .name = "Start",
                });

                g.set_deduced_type(outPin, g_ExecutionTypeId);
            }

            void on_input_change(const node_graph_context&) override {}

            void fetch_properties_descriptors(dynamic_array<node_property_descriptor>&) const override {}
            void store(data_document&, u32) const override {}
            void load(const data_document&, u32) override {}

            bool generate(const node_graph_context&,
                abstract_syntax_tree&,
                h32<ast_node>,
                const std::span<const h32<ast_node>>,
                dynamic_array<h32<ast_node>>& outputs) const override
            {
                // Execution pins don't carry a value
                outputs.emplace_back();
                return true;
            }
        };

        class consuming_node final : public node_interface
        {
        public:
            void on_create(const node_graph_context& g) override
            {
                const h32 execIn = g.add_in_pin({
                    .id = "8bd4d44c-93bb-4b3c-8ea3-300000000002"_uuid,
                    .name = "Exec In",
                });

                g.set_deduced_type(execIn, g_ExecutionTypeId);

                const h32 execOut = g.add_out_pin({
                    .id = "8bd4d44c-93bb-4b3c-8ea3-300000000003"_uuid,
                    .name = "Exec Out",
                });

                g.set_deduced_type(execOut, g_ExecutionTypeId);

                m_value = g.add_in_pin({
                    .id = "8bd4d44c-93bb-4b3c-8ea3-300000000004"_uuid,
                    .name = "Value",
                });

                g.add_out_pin({
                    .id = "8bd4d44c-93bb-4b3c-8ea3-300000000005"_uuid,
                    .name = "Result",
                });
            }

            void on_input_change(const node_graph_context&) override {}

            void fetch_properties_descriptors(dynamic_array<node_property_descriptor>&) const override {}
            void store(data_document&, u32) const override {}
            void load(const data_document&, u32) override {}

            bool generate(const node_graph_context&,
                abstract_syntax_tree& ast,
                h32<ast_node> parent,
                const std::span<const h32<ast_node>>,
                dynamic_array<h32<ast_node>>& outputs) const override
            {
                outputs.emplace_back();
                outputs.emplace_back(ast.add_node(parent, ast_f32_constant{.value = 42.f}));
                return true;
            }

        private:
            h32<node_graph_in_pin> m_value{};
        };

        class generating_f32_constant_node final : public node_interface
        {
        public:
            void on_create(const node_graph_context& g) override
            {
                g.add_out_pin({
                    .id = "4d702e17-4ec3-4902-b693-12fa1d67727e"_uuid,
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
                abstract_syntax_tree& ast,
                h32<ast_node> parent,
                const std::span<const h32<ast_node>>,
                dynamic_array<h32<ast_node>>& outputs) const override
            {
                outputs.emplace_back(ast.add_node(parent, ast_f32_constant{.value = m_value}));
                return true;
            }

        private:
            f32 m_value{};
        };

        node_descriptor make_event_node(const uuid& id, string_view name)
        {
            return {
                .id = id,
                .name = string{name},
                .instantiate = [](const any&) -> unique_ptr<node_interface> { return allocate_unique<event_node>(); },
            };
        }

        node_descriptor make_consuming_node()
        {
            return {
                .id = "8bd4d44c-93bb-4b3c-8ea3-300000000010"_uuid,
                .name = "Consume",
                .instantiate = [](const any&) -> unique_ptr<node_interface>
                { return allocate_unique<consuming_node>(); },
            };
        }

        node_descriptor make_generating_f32_constant_node()
        {
            return {
                .id = "4d702e17-4ec3-4902-b693-12fa1d67727f"_uuid,
                .name = "Float Constant",
                .instantiate = [](const any&) -> unique_ptr<node_interface>
                { return allocate_unique<generating_f32_constant_node>(); },
            };
        }

        void collect_f32_constants(
            const abstract_syntax_tree& ast, h32<ast_node> node, dynamic_array<f32>& outConstants)
        {
            const ast_node& n = ast.get(node);

            if (n.kind == ast_node_kind::f32_constant)
            {
                outConstants.emplace_back(n.node.f32.value);
            }

            for (const h32 child : ast.children(node))
            {
                collect_f32_constants(ast, child, outConstants);
            }
        }

        void collect_functions_with_body(const abstract_syntax_tree& ast, dynamic_array<hashed_string_view>& outNames)
        {
            for (const h32 child : ast.children(ast.get_root()))
            {
                const ast_node& n = ast.get(child);

                if (n.kind != ast_node_kind::function_declaration)
                {
                    continue;
                }

                bool hasBody = false;

                for (const h32 functionChild : ast.children(child))
                {
                    if (ast.get(functionChild).kind == ast_node_kind::function_body)
                    {
                        hasBody = true;
                        break;
                    }
                }

                if (hasBody)
                {
                    outNames.emplace_back(n.node.functionDecl.name);
                }
            }
        }

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

    TEST(node_graph, generate_ast_per_event)
    {
        node_graph_registry registry;

        ASSERT_TRUE(registry.register_primitive_type({
            .id = g_ExecutionTypeId,
            .name = "execution",
            .kind = node_primitive_kind::execution,
        }));

        ASSERT_TRUE(
            registry.register_node(make_event_node("8bd4d44c-93bb-4b3c-8ea3-400000000001"_uuid, "Event Spawn")));
        ASSERT_TRUE(
            registry.register_node(make_event_node("8bd4d44c-93bb-4b3c-8ea3-400000000002"_uuid, "Event Update")));
        ASSERT_TRUE(registry.register_node(make_consuming_node()));
        ASSERT_TRUE(registry.register_node(make_generating_f32_constant_node()));

        constexpr uuid spawnEventId = "8bd4d44c-93bb-4b3c-8ea3-400000000001"_uuid;
        constexpr uuid updateEventId = "8bd4d44c-93bb-4b3c-8ea3-400000000002"_uuid;
        constexpr uuid consumeNodeId = "8bd4d44c-93bb-4b3c-8ea3-300000000010"_uuid;
        constexpr uuid constantNodeId = "4d702e17-4ec3-4902-b693-12fa1d67727f"_uuid;

        node_graph g;
        g.init(registry);

        const h32 spawnEvent = g.add_node(spawnEventId);
        const h32 updateEvent = g.add_node(updateEventId);

        const h32 spawnConsumer = g.add_node(consumeNodeId);
        const h32 updateConsumer = g.add_node(consumeNodeId);

        const h32 spawnConstant = g.add_node(constantNodeId);
        const h32 updateConstant = g.add_node(constantNodeId);
        const h32 orphanConstant = g.add_node(constantNodeId);

        ASSERT_TRUE(spawnEvent && updateEvent && spawnConsumer && updateConsumer);
        ASSERT_TRUE(spawnConstant && updateConstant && orphanConstant);

        dynamic_array<h32<node_graph_out_pin>> eventOutPins;
        dynamic_array<h32<node_graph_in_pin>> consumerInPins;
        dynamic_array<h32<node_graph_out_pin>> constantOutPins;

        g.fetch_out_pins(spawnEvent, eventOutPins);
        g.fetch_out_pins(updateEvent, eventOutPins);
        g.fetch_in_pins(spawnConsumer, consumerInPins);
        g.fetch_in_pins(updateConsumer, consumerInPins);
        g.fetch_out_pins(spawnConstant, constantOutPins);
        g.fetch_out_pins(updateConstant, constantOutPins);
        g.fetch_out_pins(orphanConstant, constantOutPins);

        ASSERT_EQ(eventOutPins.size(), 2);
        ASSERT_EQ(consumerInPins.size(), 4);
        ASSERT_EQ(constantOutPins.size(), 3);

        g.connect(eventOutPins[0], consumerInPins[0]);
        g.connect(eventOutPins[1], consumerInPins[2]);
        g.connect(constantOutPins[0], consumerInPins[1]);
        g.connect(constantOutPins[1], consumerInPins[3]);

        const auto setConstantValue = [&g](h32<node_graph_node> node, f32 value)
        {
            data_document doc;
            doc.init();
            doc.child_value(doc.get_root(), "value"_hsv, property_value_wrapper{value});
            g.load(node, doc, doc.get_root());
        };

        setConstantValue(spawnConstant, 1.f);
        setConstantValue(updateConstant, 2.f);
        setConstantValue(orphanConstant, 3.f);

        abstract_syntax_tree ast;

        ASSERT_TRUE(g.generate_ast(ast));

        dynamic_array<hashed_string_view> functionNames;
        collect_functions_with_body(ast, functionNames);

        ASSERT_EQ(functionNames.size(), 2);
        ASSERT_EQ(functionNames[0], "oblo_node_graph_fn_8bd4d44c_93bb_4b3c_8ea3_400000000001"_hsv);
        ASSERT_EQ(functionNames[1], "oblo_node_graph_fn_8bd4d44c_93bb_4b3c_8ea3_400000000002"_hsv);

        const auto get_function_constants = [&ast](hashed_string_view name)
        {
            dynamic_array<f32> constants;

            for (const h32 child : ast.children(ast.get_root()))
            {
                const ast_node& n = ast.get(child);

                if (n.kind == ast_node_kind::function_declaration && n.node.functionDecl.name == name)
                {
                    collect_f32_constants(ast, child, constants);
                }
            }

            return constants;
        };

        {
            const auto constants = get_function_constants("oblo_node_graph_fn_8bd4d44c_93bb_4b3c_8ea3_400000000001"_hsv);
            ASSERT_EQ(constants.size(), 2);
            ASSERT_EQ(constants[0], 1.f);
            ASSERT_EQ(constants[1], 42.f);
        }

        {
            const auto constants = get_function_constants("oblo_node_graph_fn_8bd4d44c_93bb_4b3c_8ea3_400000000002"_hsv);
            ASSERT_EQ(constants.size(), 2);
            ASSERT_EQ(constants[0], 2.f);
            ASSERT_EQ(constants[1], 42.f);
        }

        // The orphan constant is not affected by any event, so it should not appear anywhere
        for (const h32 child : ast.children(ast.get_root()))
        {
            const ast_node& n = ast.get(child);

            if (n.kind != ast_node_kind::function_declaration)
            {
                continue;
            }

            dynamic_array<f32> constants;
            collect_f32_constants(ast, child, constants);

            for (const f32 c : constants)
            {
                ASSERT_NE(c, 3.f);
            }
        }
    }

    TEST(node_graph, generate_ast_no_events)
    {
        node_graph_registry registry;

        ASSERT_TRUE(registry.register_node(make_generating_f32_constant_node()));

        node_graph g;
        g.init(registry);

        const h32 constant = g.add_node("4d702e17-4ec3-4902-b693-12fa1d67727f"_uuid);
        ASSERT_TRUE(constant);

        abstract_syntax_tree ast;

        ASSERT_TRUE(g.generate_ast(ast));

        // Without events there is nothing to generate
        dynamic_array<hashed_string_view> functionNames;
        collect_functions_with_body(ast, functionNames);
        ASSERT_TRUE(functionNames.empty());
    }
}
