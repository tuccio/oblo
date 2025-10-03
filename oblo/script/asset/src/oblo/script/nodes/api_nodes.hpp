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

namespace oblo::api_nodes
{
    class get_time_node final : public zero_properties_node
    {
    public:
        static constexpr uuid id = "c02a725e-f8d0-46d0-80e6-78081db8d2ec"_uuid;
        static constexpr cstring_view name = "Get Time";

        void on_create(const node_graph_context& g) override
        {
            const h32 outPin = g.add_out_pin({
                .id = "5eebb786-e7b1-4c21-8da1-3b209cd9ba35"_uuid,
                .name = "Time",
            });

            g.set_deduced_type(outPin, get_node_primitive_type_id<node_primitive_kind::f32>());
        }

        void on_input_change(const node_graph_context&) override
        {
            OBLO_ASSERT(false);
        }

        bool generate(const node_graph_context&,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>>,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            const h32 call = ast.add_node(parent,
                ast_function_call{
                    .name = "__get_time"_hsv,
                });

            outputs.emplace_back(call);

            return true;
        }
    };
}