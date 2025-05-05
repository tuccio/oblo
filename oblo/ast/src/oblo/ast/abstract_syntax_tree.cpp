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

    h32<ast_node> abstract_syntax_tree::get_root() const
    {
        return {};
    }

    h32<ast_node> abstract_syntax_tree::child_next(h32<ast_node> parent, h32<ast_node> previous) const
    {
        if (!previous)
        {
            return get(parent).firstChild;
        }

        return get(previous).nextSibling;
    }

    iterator_range<abstract_syntax_tree::children_iterator> abstract_syntax_tree::children(h32<ast_node> node) const
    {
        return {
            children_iterator{*this, node, child_next(node, {})},
            children_iterator{*this, node, {}},
        };
    }

    void abstract_syntax_tree::add_child(h32<ast_node> parent, h32<ast_node> child)
    {
        OBLO_ASSERT(child);

        auto& p = get(parent);

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

    const ast_node& abstract_syntax_tree::get(h32<ast_node> node) const
    {
        return m_nodes[node.value];
    }

    ast_node& abstract_syntax_tree::get(h32<ast_node> node)
    {
        return m_nodes[node.value];
    }
}