#include <oblo/ast/abstract_syntax_tree.hpp>

namespace oblo
{
    void abstract_syntax_tree::init()
    {
        m_nodes.clear();
        auto& n = m_nodes.emplace_back();

        n.kind = ast_node_kind::root;
        n.node.root = {};

        m_stringInterner.init(0);
    }

    bool abstract_syntax_tree::is_initialized() const
    {
        return !m_nodes.empty();
    }

    h32<ast_node> abstract_syntax_tree::get_root() const
    {
        return {};
    }

    h32<ast_node> abstract_syntax_tree::get_parent(h32<ast_node> node) const
    {
        return get(node).parent;
    }

    void abstract_syntax_tree::reparent(h32<ast_node> node, h32<ast_node> newParent)
    {
        unlink_parent(node);
        link_parent(newParent, node);
    }

    void abstract_syntax_tree::unlink_subtree(h32<ast_node> root)
    {
        unlink_parent(root);
    }

    h32<ast_node> abstract_syntax_tree::child_next(h32<ast_node> parent, h32<ast_node> previous) const
    {
        if (!previous)
        {
            return get(parent).firstChild;
        }

        return get(previous).nextSibling;
    }

    h32<ast_node> abstract_syntax_tree::child_prev(h32<ast_node> parent, h32<ast_node> next) const
    {
        if (!next)
        {
            return get(parent).lastChild;
        }

        return get(next).prevSibling;
    }

    iterator_range<abstract_syntax_tree::children_iterator> abstract_syntax_tree::children(h32<ast_node> node) const
    {
        return {
            children_iterator{*this, node, child_next(node, {})},
            children_iterator{*this, node, {}},
        };
    }

    void abstract_syntax_tree::link_parent(h32<ast_node> parent, h32<ast_node> child)
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

        auto& c = get(child);
        c.parent = parent;
        c.prevSibling = p.lastChild;

        // Make sure we update the last child of the parent after prevSibling is set
        p.lastChild = child;
    }

    void abstract_syntax_tree::unlink_parent(h32<ast_node> child)
    {
        OBLO_ASSERT(child);

        auto& c = get(child);

        auto& p = get(c.parent);

        OBLO_ASSERT(p.firstChild);

        if (p.firstChild == child)
        {
            p.firstChild = c.nextSibling;
        }

        if (p.lastChild == child)
        {
            p.lastChild = c.prevSibling;
        }

        if (c.prevSibling)
        {
            auto& prev = get(c.prevSibling);
            prev.nextSibling = c.nextSibling;
        }

        if (c.nextSibling)
        {
            auto& next = get(c.nextSibling);
            next.prevSibling = c.prevSibling;
        }

        c.parent = {};
        c.nextSibling = {};
        c.prevSibling = {};
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