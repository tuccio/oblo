#include <oblo/ast/abstract_syntax_tree.hpp>

namespace oblo
{
    void abstract_syntax_tree::init()
    {
        m_nodes.clear();
        auto& n = m_nodes.emplace_back();

        n.kind = ast_node_kind::root;
        n.node.root = {};
    }

    void abstract_syntax_tree::add_child(h32<ast_node> parent, h32<ast_node> child)
    {
        OBLO_ASSERT(child);

        auto& p = get(parent);
        auto& c = get(child);

        if (!p.firstChild)
        {
            p.firstChild = child;
        }

        if (p.lastChild)
        {
            get(p.lastChild).nextSibling = child;
        }

        p.lastChild = child;
    }

    ast_node& abstract_syntax_tree::get(h32<ast_node> node)
    {
        return m_nodes[node.value];
    }
}