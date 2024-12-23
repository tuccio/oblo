#pragma once

#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/visit_result.hpp>

namespace oblo
{
    struct property_node_start
    {
    };

    struct property_node_finish
    {
    };

    struct property_array_element_start
    {
    };

    struct property_array_element_finish
    {
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

            // We skip the visit of the root, it's kind of pointless
            const auto r = index != 0 ? v(node, property_node_start{}) : visit_result::recurse;

            if (r == visit_result::recurse)
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

                    if (v(property) == visit_result::terminate)
                    {
                        return false;
                    }
                }
            }
            else if (r == visit_result::array_elements)
            {
                OBLO_ASSERT(node.isArray);

                const property_array& a = tree.arrays[node.arrayId];

                for (usize i = 0;; ++i)
                {
                    if (v(a, i, property_array_element_start{}) == visit_result::terminate)
                    {
                        break;
                    }

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

                        if (v(property) == visit_result::terminate)
                        {
                            return false;
                        }
                    }

                    v(a, i, property_array_element_finish{});
                }
            }

            v(node, property_node_finish{});

            if (r >= visit_result::sibling && node.firstSibling != 0)
            {
                if (visit_node_impl(tree, v, node.firstSibling) == visit_result::terminate)
                {
                    return false;
                }
            }

            return r != visit_result::terminate;
        }
    }

    template <typename V>
    void visit(const property_tree& tree, V&& v)
    {
        detail::visit_node_impl(tree, v, 0);
    }
}