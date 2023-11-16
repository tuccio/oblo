#pragma once

#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/data_node.hpp>
#include <oblo/properties/visit_result.hpp>

#include <string_view>

namespace oblo
{
    struct data_node_object_start
    {
    };
    struct data_node_object_finish
    {
    };

    struct data_node_value
    {
    };

    namespace detail
    {
        template <typename V>
        bool visit_node_impl(const std::span<const data_node> nodes, V& v, u32 index)
        {
            if (index >= nodes.size())
            {
                return false;
            }

            const auto& node = nodes[index];

            auto r = visit_result::recurse;

            switch (node.kind)
            {
            case data_node_kind::object:
                r = v(std::string_view{node.key, node.keyLen}, data_node_object_start{});
                break;

            case data_node_kind::value:
                r = v(std::string_view{node.key, node.keyLen}, node.value.data, node.valueKind, data_node_value{});
                break;
            }

            if (r == visit_result::recurse && node.kind == data_node_kind::object)
            {
                const auto firstChild = node.object.firstChild;

                if (firstChild != data_node::Invalid)
                {
                    if (!visit_node_impl(nodes, v, firstChild))
                    {
                        return false;
                    }
                }
            }

            v(std::string_view{node.key, node.keyLen}, data_node_object_finish{});

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