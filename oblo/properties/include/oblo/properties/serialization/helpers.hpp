#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/forward.hpp>
#include <oblo/core/span.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>

#include <initializer_list>

namespace oblo
{
    inline u32 write_child_array_initializer(
        data_document& doc, u32 parent, hashed_string_view name, std::initializer_list<property_value_wrapper> values)
    {
        const u32 node = doc.child_array(parent, name);

        for (const auto& v : values)
        {
            const u32 valueNode = doc.array_push_back(node);
            doc.make_value(valueNode, v);
        }

        return node;
    }

    template <typename T>
    inline u32 write_child_array(data_document& doc, u32 parent, hashed_string_view name, std::span<const T> values)
    {
        const u32 node = doc.child_array(parent, name);

        for (const auto& v : values)
        {
            const u32 valueNode = doc.array_push_back(node);

            if constexpr (std::is_constructible_v<property_value_wrapper, const T&>)
            {
                doc.make_value(valueNode, property_value_wrapper{v});
            }
            else if constexpr (std::is_constructible_v<string_view, const T&>)
            {
                doc.make_value(valueNode, property_value_wrapper{string_view{v}});
            }
            else
            {
                doc.make_value(valueNode, property_value_wrapper{v});
            }
        }

        return node;
    }

    template <typename T>
    inline expected<> read_array(data_document& doc, u32 node, std::span<T> values)
    {
        if (node == data_node::Invalid || !doc.is_array(node))
        {
            return "Child array not found"_err;
        }

        const u32 count = doc.children_count(node);

        if (count != values.size())
        {
            return "Child array size mismatch"_err;
        }

        auto outIt = values.begin();

        constexpr auto deduceKind = []
        {
            if constexpr (std::is_same_v<string, T>)
            {
                return property_kind::string;
            }
            else
            {
                return property_value_wrapper{T{}}.get_kind();
            }
        };

        constexpr auto kind = deduceKind();

        for (const u32 child : doc.children(node))
        {
            property_value_wrapper w;

            const expected value = doc.read_value(child);

            if (!value)
            {
                return "Child array value read failed"_err;
            }

            const std::span bytes = *value;

            if (bytes.size() != sizeof(T))
            {
                return "Child array value size mismatch"_err;
            }

            if (!w.assign_from(kind, bytes.data()))
            {
                return "Child array value kind mismatch"_err;
            }

            w.assign_to(kind, &*outIt);
        }

        return no_error;
    }

    template <typename T>
    inline expected<> read_child_array(data_document& doc, u32 parent, hashed_string_view name, std::span<T> values)
    {
        const u32 node = doc.find_child(parent, name);
        return read_array(doc, node, values);
    }
}