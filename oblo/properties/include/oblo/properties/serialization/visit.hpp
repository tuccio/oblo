#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/data_node.hpp>
#include <oblo/properties/visit_result.hpp>

namespace oblo
{
    struct data_node_object_start
    {
    };

    struct data_node_object_finish
    {
    };

    struct data_node_array_start
    {
    };

    struct data_node_array_finish
    {
    };

    struct data_node_value
    {
    };

    namespace detail
    {
        template <typename V>
        bool visit_node_impl(const deque<data_node> nodes, V& v, u32 index)
        {
            if (index >= nodes.size())
            {
                return false;
            }

            const auto& node = nodes[index];

            const auto key = string_view{node.key, node.keyLen};
            auto r = visit_result::recurse;

            const auto doRecurse = [&v, &r, &node, &nodes]
            {
                if (r == visit_result::recurse)
                {
                    const auto firstChild = node.objectOrArray.firstChild;

                    if (firstChild != data_node::Invalid)
                    {
                        if (!visit_node_impl(nodes, v, firstChild))
                        {
                            return false;
                        }
                    }
                }

                return true;
            };

            switch (node.kind)
            {
            case data_node_kind::object:
                r = v(key, data_node_object_start{});

                if (!doRecurse())
                {
                    return false;
                }

                v(key, data_node_object_finish{});

                break;

            case data_node_kind::array:
                r = v(key, data_node_array_start{});

                if (!doRecurse())
                {
                    return false;
                }

                v(key, data_node_array_finish{});
                break;

            case data_node_kind::value:
                r = v(key, node.value.data, node.valueKind, data_node_value{});
                break;

            case data_node_kind::none:
                unreachable();
            }

            if (r >= visit_result::sibling && node.nextSibling != data_node::Invalid)
            {
                if (visit_node_impl(nodes, v, node.nextSibling) == visit_result::terminate)
                {
                    return false;
                }
            }

            return r != visit_result::terminate;
        }
    }

    template <typename V>
    void visit(const data_document& doc, V&& v)
    {
        detail::visit_node_impl(doc.get_nodes(), v, doc.get_root());
    }
}