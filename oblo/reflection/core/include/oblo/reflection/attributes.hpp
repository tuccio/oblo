#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/reflection/reflection_data.hpp>

namespace oblo::reflection
{
    template <typename T>
    const T* find_attribute(const field_data& field)
    {
        constexpr auto target = get_type_id<T>();

        for (const auto& [type, ptr] : field.properties)
        {
            if (type == target)
            {
                return static_cast<const T*>(ptr);
            }
        }

        return nullptr;
    }
}