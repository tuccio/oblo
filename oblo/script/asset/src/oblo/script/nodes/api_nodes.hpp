#pragma once

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/nodes/common/ast_utils.hpp>
#include <oblo/nodes/common/execution.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/common/zero_properties_node.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>
#include <oblo/script/resources/builtin_api.hpp>

#include <optional>
#include <span>

namespace oblo::api_nodes
{
    namespace
    {
        constexpr uuid get_primitive_type_id(node_primitive_kind kind)
        {
            switch (kind)
            {
            case node_primitive_kind::execution:
                return get_node_primitive_type_id<node_primitive_kind::execution>();
            case node_primitive_kind::boolean:
                return get_node_primitive_type_id<node_primitive_kind::boolean>();
            case node_primitive_kind::i32:
                return get_node_primitive_type_id<node_primitive_kind::i32>();
            case node_primitive_kind::f32:
                return get_node_primitive_type_id<node_primitive_kind::f32>();
            case node_primitive_kind::vec3:
                return get_node_primitive_type_id<node_primitive_kind::vec3>();
            default:
                return {};
            }
        }

        constexpr hashed_string_view get_invoke_builtin_name(std::optional<node_primitive_kind> returnKind)
        {
            if (!returnKind)
            {
                return script_api::invoke_reflected_function_void;
            }

            switch (*returnKind)
            {
            case node_primitive_kind::boolean:
                return script_api::invoke_reflected_function_bool;
            case node_primitive_kind::i32:
                return script_api::invoke_reflected_function_i32;
            case node_primitive_kind::f32:
                return script_api::invoke_reflected_function_f32;
            case node_primitive_kind::vec3:
                return script_api::invoke_reflected_function_vec3;
            default:
                return script_api::invoke_reflected_function_void;
            }
        }
    }

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
        invoke_reflected_function_node(string_view name,
            std::optional<node_primitive_kind> returnKind,
            std::span<const node_primitive_kind> paramKinds,
            std::span<const cstring_view> paramNames) : m_name{name}, m_returnKind{returnKind}
        {
            const usize paramCount = paramKinds.size();
            m_paramKinds.reserve(paramCount);
            m_paramNames.reserve(paramCount);

            for (u32 i = 0; i < paramCount; ++i)
            {
                m_paramKinds.emplace_back(paramKinds[i]);
                m_paramNames.emplace_back(paramNames[i]);
            }
        }

        void on_create(const node_graph_context& g) override
        {
            add_node_execution_pins(g, true, true);

            const uuid_namespace_generator idGen{"78d2e4b6-9a10-4c5e-8f3a-2b6d1e0f9c7a"_uuid};

            string_builder builder;

            for (u32 i = 0; i < m_paramKinds.size32(); ++i)
            {
                const cstring_view realName = m_paramNames[i];
                const cstring_view idName = realName.empty() ? realName : builder.clear().format("#{}", i).view();

                const h32 pin = g.add_in_pin({
                    .id = idGen.generate(idName.as<string_view>()),
                    .name = realName.as<string>(),
                });

                g.set_deduced_type(pin, get_primitive_type_id(m_paramKinds[i]));

                m_inPins.emplace_back(pin);
            }

            if (m_returnKind)
            {
                const h32 outPin = g.add_out_pin({
                    .id = idGen.generate("result"_hsv),
                    .name = "Result",
                });

                g.set_deduced_type(outPin, get_primitive_type_id(*m_returnKind));
            }
        }

        void on_input_change(const node_graph_context&) override {}

        bool generate(const node_graph_context& g,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            add_node_execution_ast_node(outputs);

            const h32 call = ast.add_node(parent,
                ast_function_call{
                    .name = get_invoke_builtin_name(m_returnKind),
                });

            {
                const h32 fqnParameter = ast.add_node(call,
                    ast_function_argument{
                        .name = "fqn"_hsv,
                    });

                ast.add_node(fqnParameter,
                    ast_string_constant{
                        .value = hashed_string_view{m_name},
                    });
            }

            {
                const h32 countParameter = ast.add_node(call,
                    ast_function_argument{
                        .name = "count"_hsv,
                    });

                ast.add_node(countParameter,
                    ast_u32_constant{
                        .value = m_paramKinds.size32(),
                    });
            }

            {
                const h32 argsParameter = ast.add_node(call,
                    ast_function_argument{
                        .name = "args"_hsv,
                    });

                if (m_paramKinds.empty())
                {
                    ast.add_node(argsParameter, ast_null{});
                }
                else
                {
                    const h32 arrayDecl = ast.add_node(parent,
                        ast_array_declaration{
                            .elementType = script_api::const_void_ptr_t,
                            .size = m_paramKinds.size32(),
                        });

                    for (u32 i = 0; i < m_paramKinds.size32(); ++i)
                    {
                        const h32 addressOf = ast.add_node(arrayDecl, ast_address_of{});

                        h32 valueExpression = inputs[i + 1];

                        if (valueExpression)
                        {
                            const auto inType = g.get_incoming_type(m_inPins[i]);
                            const auto paramType = get_primitive_type_id(m_paramKinds[i]);

                            if (inType != paramType)
                            {
                                valueExpression =
                                    ast_utils::make_type_conversion(ast, valueExpression, inType, paramType);
                            }

                            ast.reparent(valueExpression, addressOf);
                        }
                        else
                        {
                            ast_utils::make_default_value_child(ast, addressOf, m_paramKinds[i]);
                        }
                    }

                    ast.reparent(arrayDecl, argsParameter);
                }
            }

            if (m_returnKind)
            {
                outputs.emplace_back(call);
            }

            return true;
        }

    private:
        string m_name;
        std::optional<node_primitive_kind> m_returnKind;
        dynamic_array<node_primitive_kind> m_paramKinds;
        dynamic_array<string> m_paramNames;
        dynamic_array<h32<node_graph_in_pin>> m_inPins;
    };
}