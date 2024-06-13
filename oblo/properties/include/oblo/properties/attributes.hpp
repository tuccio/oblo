#pragma once

#include <oblo/properties/property_tree.hpp>

namespace oblo
{
    template <typename T>
    const T* find_attribute(const property_tree& tree, const property_node& node)
    {
        constexpr auto target = get_type_id<T>();

        for (u32 i = node.firstAttribute; i < node.lastAttribute; ++i)
        {
            const auto& [type, ptr] = tree.attributes[i];

            if (type == target)
            {
                return static_cast<const T*>(ptr);
            }
        }

        return nullptr;
    }

    template <typename T>
    const T* find_attribute(const property_tree& tree, const property& prop)
    {
        constexpr auto target = get_type_id<T>();

        for (u32 i = prop.firstAttribute; i < prop.lastAttribute; ++i)
        {
            const auto& [type, ptr] = tree.attributes[i];

            if (type == target)
            {
                return static_cast<const T*>(ptr);
            }
        }

        return nullptr;
    }
}