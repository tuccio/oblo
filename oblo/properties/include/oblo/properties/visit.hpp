#pragma once

#include <oblo/properties/property_tree.hpp>

namespace oblo
{
    enum property_visit_result : u8
    {
        terminate,
        sibling,
        recurse,
    };

    namespace detail
    {
        template <typename V>
        bool visit_node_impl(const property_tree& tree, V& v, u32 index)
        {
            if (index >= tree.nodes.size())
            {
                return false;
            }

            const auto& node = tree.nodes[index];
            const auto r = v(node);

            if (r == property_visit_result::recurse)
            {
                if (node.firstChild != 0)
                {
                    if (!visit_node_impl(tree, v, node.firstChild))
                    {
                        return false;
                    }
                }

                for (u32 propertyIndex = node.firstProperty; propertyIndex != node.lastProperty; ++propertyIndex)
                {
                    const auto& property = tree.properties[propertyIndex];

                    if (v(property) == property_visit_result::terminate)
                    {
                        return false;
                    }
                }
            }

            if (r >= property_visit_result::sibling && node.firstSibling != 0)
            {
                if (visit_node_impl(tree, v, node.firstSibling) == property_visit_result::terminate)
                {
                    return false;
                }
            }

            return r != property_visit_result::terminate;
        }
    }

    template <typename V>
    void visit(const property_tree& tree, V&& v)
    {
        detail::visit_node_impl(tree, v, 0);
    }
}