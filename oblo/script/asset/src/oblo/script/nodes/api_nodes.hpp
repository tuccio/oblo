#pragma once

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/nodes/common/ast_utils.hpp>
#include <oblo/nodes/common/execution.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/common/zero_properties_node.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>
#include <oblo/script/resources/builtin_api.hpp>

namespace oblo::api_nodes
{
    class get_time_node final : public zero_properties_node
    {
    public:
        static constexpr uuid id = "c02a725e-f8d0-46d0-80e6-78081db8d2ec"_uuid;
        static constexpr cstring_view name = "Get Time";
        static constexpr cstring_view category = "World";

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
                    .name = script_api::get_time,
                });

            outputs.emplace_back(call);

            return true;
        }
    };

    class invoke_reflected_function_node final : public zero_properties_node
    {
    public:
        explicit invoke_reflected_function_node(string_view name) : m_name{name} {}

        void on_create(const node_graph_context& g) override
        {
            add_node_execution_pins(g, true, true);
        }

        void on_input_change(const node_graph_context&) override {}

        bool generate(const node_graph_context&,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>>,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            add_node_execution_ast_node(outputs);

            const h32 call = ast.add_node(parent,
                ast_function_call{
                    .name = script_api::invoke_reflected_function,
                });

            {
                const h32 nameParameter = ast.add_node(call,
                    ast_function_argument{
                        .name = "name"_hsv,
                    });

                ast.add_node(nameParameter,
                    ast_string_constant{
                        .value = hashed_string_view{m_name},
                    });
            }

            return true;
        }

    private:
        string m_name;
    };
}