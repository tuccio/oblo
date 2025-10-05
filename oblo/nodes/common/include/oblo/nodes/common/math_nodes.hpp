#pragma once

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/nodes/common/ast_utils.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/common/zero_properties_node.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>

namespace oblo::math_nodes
{
    template <typename Derived>
    class f32_intrinsic_function_node : public zero_properties_node
    {
    public:
        void on_create(const node_graph_context& g) override
        {
            const h32 inPin = g.add_in_pin({
                .id = "7c82cf9a-25fd-4a67-8736-abdd362a9032"_uuid,
                .name = "Input",
            });

            const h32 outPin = g.add_out_pin({
                .id = "9d1a9454-f023-40c6-af76-d2fbea316e72"_uuid,
                .name = "Output",
            });

            g.set_deduced_type(inPin, get_node_primitive_type_id<node_primitive_kind::f32>());
            g.set_deduced_type(outPin, get_node_primitive_type_id<node_primitive_kind::f32>());
        }

        void on_input_change(const node_graph_context&) override {}

        bool generate(const node_graph_context&,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            if (inputs.size() != 1)
            {
                return false;
            }

            const h32 call = ast.add_node(parent,
                ast_function_call{
                    .name = Derived::intrisic_id,
                });

            ast.reparent(inputs[0], call);

            outputs.emplace_back(call);

            return true;
        }
    };

    class sine_node final : public f32_intrinsic_function_node<sine_node>
    {
    public:
        static constexpr uuid id = "23f1f6d1-e626-4e56-8ac7-ac3700569268"_uuid;
        static constexpr cstring_view name = "Sine";
        static constexpr cstring_view category = "Math";
        static constexpr hashed_string_view intrisic_id = "__intrin_sin"_hsv;
    };

    class cosine_node final : public f32_intrinsic_function_node<cosine_node>
    {
    public:
        static constexpr uuid id = "722e8cf8-c8f7-4ad6-8652-791574f4fc86"_uuid;
        static constexpr cstring_view name = "Cosine";
        static constexpr cstring_view category = "Math";
        static constexpr hashed_string_view intrisic_id = "__intrin_cos"_hsv;
    };

    class tangent_node final : public f32_intrinsic_function_node<tangent_node>
    {
    public:
        static constexpr uuid id = "4b45ca3d-894b-4c36-bebc-ce8753658dc5"_uuid;
        static constexpr cstring_view name = "Tangent";
        static constexpr cstring_view category = "Math";
        static constexpr hashed_string_view intrisic_id = "__intrin_tan"_hsv;
    };

    class arctangent_node final : public f32_intrinsic_function_node<arctangent_node>
    {
    public:
        static constexpr uuid id = "db40d485-ae43-4dbf-b13b-9158dd22b372"_uuid;
        static constexpr cstring_view name = "Arctangent";
        static constexpr cstring_view category = "Math";
        static constexpr hashed_string_view intrisic_id = "__intrin_atan"_hsv;
    };
}