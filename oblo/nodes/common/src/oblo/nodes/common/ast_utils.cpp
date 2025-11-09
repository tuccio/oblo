#include <oblo/nodes/common/ast_utils.hpp>

#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/uuid.hpp>
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

        // TODO (#79): Implement type conversions
        (void) ast;
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
            const h32 n = ast.add_node(parent, ast_compound{});
            ast.add_node(n, ast_f32_constant{.value = 0.f});
            ast.add_node(n, ast_f32_constant{.value = 0.f});
            ast.add_node(n, ast_f32_constant{.value = 0.f});
            return n;
        }
        default:
            OBLO_ASSERT(false);
            return {};
        }
    }
}