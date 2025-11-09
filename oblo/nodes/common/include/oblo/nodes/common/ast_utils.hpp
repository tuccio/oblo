#include <oblo/core/handle.hpp>

namespace oblo
{
    class abstract_syntax_tree;
    struct ast_node;
    struct uuid;
    enum class node_primitive_kind : u8;
}

namespace oblo::ast_utils
{
    h32<ast_node> make_type_conversion(
        abstract_syntax_tree& ast, h32<ast_node> expression, const uuid& type, const uuid& targetType);

    h32<ast_node> make_default_value_child(abstract_syntax_tree& ast, h32<ast_node> parent, node_primitive_kind type);
}