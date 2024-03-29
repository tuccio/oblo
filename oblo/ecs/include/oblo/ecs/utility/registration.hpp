#pragma once

#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/ecs/traits.hpp>
#include <oblo/ecs/type_registry.hpp>

#include <memory>

namespace oblo::ecs
{
    template <typename T>
    constexpr auto make_component_type_desc()
    {
        return component_type_desc{
            .type = get_type_id<T>(),
            .size = u32(sizeof(T)),
            .alignment = u32(alignof(T)),
            .create = [](void* dst, usize count) { std::uninitialized_value_construct_n(static_cast<T*>(dst), count); },
            .destroy =
                [](void* dst, usize count)
            {
                T* outIt = static_cast<T*>(dst);
                T* const end = outIt + count;

                for (; outIt != end; ++outIt)
                {
                    outIt->~T();
                }
            },
            .move =
                [](void* dst, void* src, usize count)
            {
                T* outIt = static_cast<T*>(dst);
                T* inIt = static_cast<T*>(src);
                T* const end = outIt + count;

                for (; outIt != end; ++outIt, ++inIt)
                {
                    new (outIt) T(std::move(*inIt));
                }
            },
            .moveAssign =
                [](void* dst, void* src, usize count)
            {
                T* outIt = static_cast<T*>(dst);
                T* inIt = static_cast<T*>(src);
                T* const end = outIt + count;

                for (; outIt != end; ++outIt, ++inIt)
                {
                    *outIt = std::move(*inIt);
                }
            },
        };
    }

    template <typename T>
    constexpr auto make_tag_type_desc()
    {
        return tag_type_desc{.type = get_type_id<T>()};
    }

    template <typename T>
    auto register_type(type_registry& registry)
    {
        if constexpr (is_tag_v<T>)
        {
            return registry.register_tag(make_tag_type_desc<T>());
        }
        else
        {
            return registry.register_component(make_component_type_desc<T>());
        }
    }
}