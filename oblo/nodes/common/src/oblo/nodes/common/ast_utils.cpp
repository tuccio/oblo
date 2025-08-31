#include <oblo/nodes/common/ast_utils.hpp>

#include <oblo/core/uuid.hpp>

namespace oblo::ast_utils
{
    h32<ast_node> make_type_conversion(
        abstract_syntax_tree& ast, h32<ast_node> expression, const uuid& type, const uuid& targetType)
    {
        if (type == targetType)
        {
            return expression;
        }

        // TODO: Actually convert types
        (void) ast;
        OBLO_ASSERT(false);

        return expression;
    }
}