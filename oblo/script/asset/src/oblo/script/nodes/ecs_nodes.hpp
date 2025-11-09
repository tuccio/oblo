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
    template <node_primitive_kind Kind>
    class get_component_property_node final : public zero_properties_node
    {
    public:
        static consteval uuid get_type_uuid()
        {
            return get_node_primitive_type_id<Kind>();
        }

        explicit get_component_property_node(type_id componentType, string_view propertyPath) :
            m_componentType{componentType}, m_propertyPath{propertyPath}
        {
        }

        void on_create(const node_graph_context& g) override
        {
            const h32 outPin = g.add_out_pin({
                .id = "114f1733-9f3a-470c-9a50-8585db310c49"_uuid,
                .name = "Get",
            });

            g.set_deduced_type(outPin, get_type_uuid());
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
                    .name = script_api::ecs::get_property_f32,
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
    };

    template <node_primitive_kind Kind>
    class set_component_property_node_base : public zero_properties_node
    {
    public:
        static consteval uuid get_type_uuid()
        {
            return get_node_primitive_type_id<Kind>();
        }

        explicit set_component_property_node_base(type_id componentType, string_view propertyPath) :
            m_componentType{componentType}, m_propertyPath{propertyPath}
        {
        }

        void on_create(const node_graph_context& g) override
        {
            m_input = g.add_in_pin({
                .id = "efa8ae2b-c1dd-4741-a280-47e970977fa3"_uuid,
                .name = "Set",
            });

            g.set_deduced_type(m_input, get_type_uuid());
        }

        void on_input_change(const node_graph_context&) override {}

        bool generate(const node_graph_context& g,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>&) const override
        {
            const h32 callNode = ast.add_node(parent,
                ast_function_call{
                    .name = script_api::ecs::set_property_f32,
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

                // Initialize the value on the stack
                h32 valueExpression = inputs[0];

                if (valueExpression)
                {
                    const auto inType = g.get_incoming_type(m_input);

                    if (inType != get_type_uuid())
                    {
                        valueExpression =
                            ast_utils::make_type_conversion(ast, valueExpression, inType, get_type_uuid());
                    }

                    ast.reparent(valueExpression, valueParameter);
                }
                else
                {
                    ast_utils::make_default_value_child(ast, valueParameter, Kind);
                }
            }

            return true;
        }

    protected:
        type_id m_componentType;
        string m_propertyPath;
        h32<node_graph_in_pin> m_input{};
    };

    template <node_primitive_kind Kind>
    class set_component_property_node final : public set_component_property_node_base<Kind>
    {
        using set_component_property_node_base<Kind>::set_component_property_node_base;
    };

    template <>
    class set_component_property_node<node_primitive_kind::vec3> final :
        public set_component_property_node_base<node_primitive_kind::vec3>
    {
    public:
        using set_component_property_node_base<node_primitive_kind::vec3>::set_component_property_node_base;

        void on_create(const node_graph_context& g) override
        {
            m_input = g.add_in_pin({
                .id = "4cc2d571-5cee-4c9f-bd63-b88d965d7a98"_uuid,
                .name = "xyz",
            });

            m_components[0] = g.add_in_pin({
                .id = "912d9baa-5097-4c30-901e-062dead90db1"_uuid,
                .name = "x",
            });

            m_components[1] = g.add_in_pin({
                .id = "98edf5fa-5c2d-4821-962e-7c9ce2047ef0"_uuid,
                .name = "y",
            });

            m_components[2] = g.add_in_pin({
                .id = "b7177b65-d6e7-496a-a841-0efacd47fd85"_uuid,
                .name = "z",
            });

            g.set_deduced_type(m_input, get_type_uuid());
            g.set_deduced_type(m_components[0], get_node_primitive_type_id<node_primitive_kind::f32>());
            g.set_deduced_type(m_components[1], get_node_primitive_type_id<node_primitive_kind::f32>());
            g.set_deduced_type(m_components[2], get_node_primitive_type_id<node_primitive_kind::f32>());
        }

        bool generate(const node_graph_context& g,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>&) const override
        {
            const h32 callNode = ast.add_node(parent,
                ast_function_call{
                    .name = script_api::ecs::set_property_vec3,
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
                const h32 maskParameter = ast.add_node(callNode,
                    ast_function_argument{
                        .name = "mask"_hsv,
                    });

                u32 mask = 0;

                if (inputs[0])
                {
                    mask = 0b111;
                }
                else
                {
                    for (u32 i = 1; i < inputs.size(); ++i)
                    {
                        mask |= u32{bool{inputs[i]}} << (i - 1);
                    }
                }

                ast.add_node(maskParameter,
                    ast_u32_constant{
                        .value = mask,
                    });
            }

            {
                const h32 valueParameter = ast.add_node(callNode,
                    ast_function_argument{
                        .name = "value"_hsv,
                    });

                // Initialize the value on the stack
                h32 valueExpression = inputs[0];

                if (valueExpression)
                {
                    const auto inType = g.get_incoming_type(m_input);

                    if (inType != get_type_uuid())
                    {
                        valueExpression =
                            ast_utils::make_type_conversion(ast, valueExpression, inType, get_type_uuid());
                    }

                    ast.reparent(valueExpression, valueParameter);
                }
                else
                {
                    valueExpression =
                        ast_utils::make_default_value_child(ast, valueParameter, node_primitive_kind::vec3);
                }

                if (!valueExpression)
                {
                    return false;
                }

                const auto& vec3Node = ast.get(valueExpression);

                if (vec3Node.kind != ast_node_kind::compound)
                {
                    return false;
                }

                buffered_array<h32<ast_node>, 3> componentsExpr;

                for (const h32 c : ast.children(valueExpression))
                {
                    componentsExpr.emplace_back(c);
                }

                if (componentsExpr.size() != 3)
                {
                    return false;
                }

                for (u32 i = 0; i < 3; ++i)
                {
                    const h32 componentIn = inputs[i + 1];
                    const h32 componentExpr = componentsExpr[i];

                    if (!componentExpr)
                    {
                        return false;
                    }

                    if (componentIn)
                    {
                        ast.swap_subtrees(componentIn, componentExpr);
                    }
                }
            }

            return true;
        }

    private:
        h32<node_graph_in_pin> m_components[3]{};
    };
}