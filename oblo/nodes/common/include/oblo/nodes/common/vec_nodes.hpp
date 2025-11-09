#pragma once

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/nodes/common/ast_utils.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/common/zero_properties_node.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_primitive_type.hpp>

namespace oblo::vec_nodes
{
    static constexpr cstring_view category = "Vector";

    class make_vec3_node final : public zero_properties_node
    {
    public:
        static constexpr uuid id = "baa0b906-9e34-415d-aba6-b7c17a7acc40"_uuid;
        static constexpr cstring_view name = "Make vec3";
        static constexpr cstring_view category = vec_nodes::category;

        void on_create(const node_graph_context& g) override
        {
            m_inputs[0] = g.add_in_pin({
                .id = "994407c2-9f38-4063-be17-547b1228aec0"_uuid,
                .name = "x",
            });

            m_inputs[1] = g.add_in_pin({
                .id = "349d5834-a109-4f93-93c9-0a7186adab09"_uuid,
                .name = "y",
            });

            m_inputs[2] = g.add_in_pin({
                .id = "8beacd5d-e5d6-4c9c-8939-5f01e98a09ed"_uuid,
                .name = "z",
            });

            g.set_deduced_type(m_inputs[0], get_node_primitive_type_id<node_primitive_kind::f32>());
            g.set_deduced_type(m_inputs[1], get_node_primitive_type_id<node_primitive_kind::f32>());
            g.set_deduced_type(m_inputs[2], get_node_primitive_type_id<node_primitive_kind::f32>());

            m_output = g.add_out_pin({
                .id = "08b11479-4758-4b52-95c5-9e5b4d3849c4"_uuid,
                .name = "xyz",
            });

            g.set_deduced_type(m_output, get_node_primitive_type_id<node_primitive_kind::vec3>());
        }

        void on_input_change(const node_graph_context&) override {}

        bool generate(const node_graph_context& g,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            const h32 out = ast.add_node(parent, ast_compound{});

            for (u32 i = 0; i < 3; ++i)
            {
                h32 valueExpression = inputs[i];

                if (valueExpression)
                {
                    const auto inType = g.get_incoming_type(m_inputs[i]);

                    constexpr uuid f32Type = get_node_primitive_type_id<node_primitive_kind::f32>();

                    if (inType != f32Type)
                    {
                        valueExpression = ast_utils::make_type_conversion(ast, valueExpression, inType, f32Type);
                    }

                    ast.reparent(valueExpression, out);
                }
                else
                {
                    ast_utils::make_default_value_child(ast, out, node_primitive_kind::f32);
                }
            }

            outputs.emplace_back(out);
            return true;
        }

    private:
        h32<node_graph_in_pin> m_inputs[3]{};
        h32<node_graph_out_pin> m_output;
    };
}