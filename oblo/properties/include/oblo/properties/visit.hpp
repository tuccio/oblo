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

            auto r = visit_result::recurse;

            if (node.isArray)
            {
                const property_array& a = tree.arrays[node.arrayId];

                const auto visitElement = [&]
                {
                    const auto numProperties = node.lastProperty - node.firstProperty;

                    if (numProperties == 2)
                    {
                        const auto elementProperty = node.lastProperty - 1;

                        const auto& property = tree.properties[elementProperty];
                        OBLO_ASSERT(property.name == notable_properties::array_element);

                        if (v(property) == visit_result::terminate)
                        {
                            return false;
                        }
                    }
                    else if (node.firstChild != 0)
                    {
                        if (!visit_node_impl(tree, v, node.firstChild))
                        {
                            return false;
                        }
                    }

                    return true;
                };

                r = v(node, a, visitElement);
            }
            else
            {
                if (index != 0)
                {
                    r = v(node, property_node_start{});
                }

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

                v(node, property_node_finish{});
            }

            /* else if (r == visit_result::array_elements)
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
             }*/

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