#pragma once

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/nodes/common/ast_utils.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/common/zero_properties_node.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>
#include <oblo/script/resources/builtin_api.hpp>

namespace oblo::ecs_nodes
{
    class get_component_property_node final : public zero_properties_node
    {
    public:
        explicit get_component_property_node(type_id componentType, string_view propertyPath, const uuid& type) :
            m_componentType{componentType}, m_propertyPath{propertyPath}, m_type{type}
        {
        }

        void on_create(const node_graph_context& g) override
        {
            const h32 outPin = g.add_out_pin({
                .id = "114f1733-9f3a-470c-9a50-8585db310c49"_uuid,
                .name = "Get",
            });

            g.set_deduced_type(outPin, m_type);
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
            const h32 getPropertyCall = ast.add_node(parent,
                ast_function_call{
                    .name = script_api::ecs::get_property,
                });

            {
                const h32 propertyParameter = ast.add_node(getPropertyCall,
                    ast_function_argument{
                        .name = "componentType"_hsv,
                    });

                ast.add_node(propertyParameter,
                    ast_string_constant{
                        .value = m_componentType.name,
                    });
            }

            {
                const h32 propertyParameter = ast.add_node(getPropertyCall,
                    ast_function_argument{
                        .name = "property"_hsv,
                    });

                ast.add_node(propertyParameter,
                    ast_string_constant{
                        .value = hashed_string_view{m_propertyPath},
                    });
            }

            outputs.emplace_back(getPropertyCall);

            return true;
        }

    private:
        type_id m_componentType;
        string m_propertyPath;
        uuid m_type;
    };

    class set_component_property_node final : public zero_properties_node
    {
    public:
        explicit set_component_property_node(type_id componentType, string_view propertyPath, const uuid& type) :
            m_componentType{componentType}, m_propertyPath{propertyPath}, m_type{type}
        {
        }

        void on_create(const node_graph_context& g) override
        {
            m_input = g.add_in_pin({
                .id = "efa8ae2b-c1dd-4741-a280-47e970977fa3"_uuid,
                .name = "Set",
            });
        }

        void on_input_change(const node_graph_context&) override {}

        bool generate(const node_graph_context& g,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>&) const override
        {
            constexpr hashed_string_view setPropertyDataName = "__ecs_set_property_data"_hsv;

            const h32 callNode = ast.add_node(parent,
                ast_function_call{
                    .name = script_api::ecs::set_property,
                });

            {
                const h32 propertyParameter = ast.add_node(callNode,
                    ast_function_argument{
                        .name = "componentType"_hsv,
                    });

                ast.add_node(propertyParameter,
                    ast_string_constant{
                        .value = m_componentType.name,
                    });
            }

            {
                const h32 propertyParameter = ast.add_node(callNode,
                    ast_function_argument{
                        .name = "property"_hsv,
                    });

                ast.add_node(propertyParameter,
                    ast_string_constant{
                        .value = hashed_string_view{m_propertyPath},
                    });
            }

            {
                const h32 valueParameter = ast.add_node(callNode,
                    ast_function_argument{
                        .name = "value"_hsv,
                    });

                ast.add_node(valueParameter, ast_variable_reference{.name = setPropertyDataName});
            }

            // Add this last so it gets executed before the call
            {
                const h32 dataVarDecl = ast.add_node(parent,
                    ast_variable_declaration{
                        .name = setPropertyDataName,
                    });

                const h32 dataVarDef = ast.add_node(dataVarDecl, ast_variable_definition{});

                // Initialize the value on the stack
                h32 valueExpression = inputs[0];

                const auto inType = g.get_incoming_type(m_input);

                if (inType != m_type)
                {
                    valueExpression = ast_utils::make_type_conversion(ast, valueExpression, inType, m_type);
                }

                ast.reparent(valueExpression, dataVarDef);
            }

            return true;
        }

    private:
        type_id m_componentType;
        string m_propertyPath;
        uuid m_type{};
        h32<node_graph_in_pin> m_input{};
    };
}