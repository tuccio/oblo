#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/common/zero_properties_node.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>

namespace oblo
{
    // Binary operators that operate on types, they may need to promote if the types are not matching, so that all
    // operands and result type will be the same
    namespace uniform_binary_operator
    {
        inline void on_input_change(const node_graph_context& g,
            h32<node_graph_in_pin> lhs,
            h32<node_graph_in_pin> rhs,
            h32<node_graph_out_pin> out,
            uuid defaultIfMissing)
        {
            const uuid lhsInType = g.get_incoming_type(lhs);
            const uuid rhsInType = g.get_incoming_type(rhs);

            const uuid outType = g.find_promotion_rule(lhsInType.is_nil() ? defaultIfMissing : defaultIfMissing,
                rhsInType.is_nil() ? defaultIfMissing : defaultIfMissing);

            g.set_deduced_type(lhs, outType);
            g.set_deduced_type(rhs, outType);
            g.set_deduced_type(out, outType);
        }
    }

    template <typename Base>
    class binary_operator_base : public zero_properties_node
    {
    public:
        void on_create(const node_graph_context& g) override
        {
            m_lhs = g.add_in_pin({
                .id = "fcd0651c-6bf6-4933-84a3-d6ca05a60ae6"_uuid,
                .name = "A",
            });

            m_rhs = g.add_in_pin({
                .id = "fe917388-c132-420b-84df-bf1108a8992c"_uuid,
                .name = "B",
            });

            m_out = g.add_out_pin({
                .id = "4fe1662d-42c3-46d8-8351-57b23e33cb3c"_uuid,
                .name = "Result",
            });
        }

        void on_input_change(const node_graph_context& g) override
        {
            return static_cast<Base*>(this)->on_input_change(g);
        }

        bool generate(const node_graph_context& g,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            return static_cast<const Base*>(this)->generate(g, ast, parent, inputs, outputs);
        }

    protected:
        h32<node_graph_in_pin> m_lhs{};
        h32<node_graph_in_pin> m_rhs{};
        h32<node_graph_out_pin> m_out{};
    };

    class add_operator final : public binary_operator_base<add_operator>
    {
    public:
        static constexpr uuid id = "13514366-b0af-4a25-a4c6-384bd7277a35"_uuid;
        static constexpr cstring_view name = "Add";

        void on_input_change(const node_graph_context& g) const
        {
            constexpr uuid defaultType = get_node_primitive_type_id<node_primitive_kind::f32>();

            uniform_binary_operator::on_input_change(g, m_lhs, m_rhs, m_out, defaultType);
        }

        bool generate([[maybe_unused]] const node_graph_context& g,
            [[maybe_unused]] abstract_syntax_tree& ast,
            [[maybe_unused]] h32<ast_node> parent,
            [[maybe_unused]] const std::span<const h32<ast_node>> inputs,
            [[maybe_unused]] dynamic_array<h32<ast_node>>& outputs) const
        {
#if 0
            if (inputs.size() != 2)
            {
                return false;
            }

            const auto lhsType = g.get_incoming_type(m_lhs);
            const auto rhsType = g.get_incoming_type(m_rhs);

            const auto outType = g.get_deduced_type(m_out);

            h32<ast_node> lhsExpr = inputs[0];
            h32<ast_node> rhsExpr = inputs[1];

            if (lhsType != outType)
            {
                lhsExpr = generate_conversion_expression(ast, lhsExpr, lhsType, outType);
            }

            if (rhsType != outType)
            {
                rhsExpr = generate_conversion_expression(ast, rhsExpr, rhsType, outType);
            }

            if (!lhsExpr || !rhsExpr)
            {
                return false;
            }

            ast_binary_operator_kind kind;

            if (outType == get_node_primitive_type_id<node_primitive_kind::f32>())
            {
                kind = ast_binary_operator_kind::add_f32;
            }
            else
            {
                return false;
            }

            const h32 outExpr = ast.add_node(parent,
                ast_binary_operator{
                    .op = kind,
                });

            ast.reparent(lhsExpr, outExpr);
            ast.reparent(rhsExpr, outExpr);

            outputs.emplace_back(outExpr);

#endif

            return true;
        }
    };

    class mul_operator final : public binary_operator_base<mul_operator>
    {
    public:
        static constexpr uuid id = "f8b0b90c-3f18-4235-b694-c45f9657a317"_uuid;
        static constexpr cstring_view name = "Multiply";

        void on_input_change(const node_graph_context& g) const
        {
            constexpr uuid defaultType = get_node_primitive_type_id<node_primitive_kind::f32>();

            uniform_binary_operator::on_input_change(g, m_lhs, m_rhs, m_out, defaultType);
        }

        bool generate([[maybe_unused]] const node_graph_context& g,
            [[maybe_unused]] abstract_syntax_tree& ast,
            [[maybe_unused]] h32<ast_node> parent,
            [[maybe_unused]] const std::span<const h32<ast_node>> inputs,
            [[maybe_unused]] dynamic_array<h32<ast_node>>& outputs) const
        {
            return true;
        }
    };
}