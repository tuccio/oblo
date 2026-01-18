#include <oblo/nodes/common/ast_utils.hpp>

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/node_primitive_type.hpp>

namespace oblo::ast_utils
{
    h32<ast_node> make_type_conversion(
        abstract_syntax_tree& ast, h32<ast_node> expression, const uuid& type, const uuid& targetType)
    {
        if (type == targetType)
        {
            return expression;
        }

        constexpr uuid f32Type = get_node_primitive_type_id<node_primitive_kind::f32>();
        constexpr uuid vec3Type = get_node_primitive_type_id<node_primitive_kind::vec3>();

        if (targetType == vec3Type)
        {
            if (type == f32Type)
            {
                const u32 uniqueId = ast.get_nodes_count();

                // Splats the expression to the 3 components
                string_builder builder;
                builder.format("__vec3_to_f32_{}", uniqueId);

                const auto varName = builder.as<hashed_string_view>();

                const h32 vec3Node = ast.add_node({}, ast_construct_type{.type = "vec3"_hsv});

                // Add y and z after by referencing the variable
                for (u32 i = 0; i < 3; ++i)
                {
                    const auto arg = ast.add_node(vec3Node, ast_function_argument{});
                    ast.add_node(arg, ast_variable_reference{.name = varName});
                }

                // Make a variable with the expression, push it after the references because order is reverse
                const h32 decl = ast.add_node(vec3Node, ast_variable_declaration{.name = varName});
                const h32 def = ast.add_node(decl, ast_variable_definition{});
                ast.reparent(expression, def);

                return vec3Node;
            }
        }

        // TODO (#79): Implement type conversions
        OBLO_ASSERT(false);

        return expression;
    }

    h32<ast_node> make_default_value_child(abstract_syntax_tree& ast, h32<ast_node> parent, node_primitive_kind type)
    {
        switch (type)
        {
        case node_primitive_kind::boolean:
            return ast.add_node(parent, ast_i32_constant{.value = 0});

        case node_primitive_kind::i32:
            return ast.add_node(parent, ast_i32_constant{.value = 0});

        case node_primitive_kind::f32:
            return ast.add_node(parent, ast_f32_constant{.value = 0.f});

        case node_primitive_kind::vec3: {
            const h32 n = ast.add_node(parent, ast_construct_type{.type = "vec3"_hsv});

            for (u32 i = 0; i < 3; ++i)
            {
                const auto arg = ast.add_node(n, ast_function_argument{});
                ast.add_node(arg, ast_f32_constant{.value = 0.f});
            }
            return n;
        }
        default:
            OBLO_ASSERT(false);
            return {};
        }
    }
}