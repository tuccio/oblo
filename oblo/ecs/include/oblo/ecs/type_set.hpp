#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/ecs/limits.hpp>

#include <compare>

namespace oblo::ecs
{
    class type_registry;

    struct type_set
    {
        template <typename T>
        void add(h32<T> index)
        {
            OBLO_ASSERT(index);
            const u64 maskOffset = index.value % BitsPerBlock;
            const u64 mask = u64(1) << maskOffset;
            const u64 block = index.value / BitsPerBlock;
            bitset[block] |= mask;
        }

        constexpr bool is_empty() const
        {
            return *this == type_set{};
        }

        template <typename T>
        constexpr bool contains(h32<T> index) const
        {
            OBLO_ASSERT(index);
            const u64 maskOffset = index.value % BitsPerBlock;
            const u64 mask = u64(1) << maskOffset;
            const u64 block = index.value / BitsPerBlock;
            const u64 res = bitset[block] & mask;
            return res != 0;
        }

        constexpr auto operator<=>(const type_set&) const = default;

        static constexpr u32 BitsPerBlock{64u};
        static constexpr u32 BlocksCount{round_up_div(MaxComponentTypes, BitsPerBlock)};

        u64 bitset[BlocksCount];
    };

    struct component_and_tags_sets
    {
        type_set components;
        type_set tags;
    };

    template <typename... ComponentsOrTags>
    component_and_tags_sets make_type_sets(const type_registry& typeRegistry)
    {
        component_and_tags_sets sets{};

        const auto doAdd = [&typeRegistry, &sets]<typename T>(const T*)
        {
            if constexpr (is_tag_v<T>)
            {
                const auto tag = typeRegistry.find_tag<T>();

                if (tag)
                {
                    sets.tags.add(tag);
                }
            }
            else
            {
                const auto component = typeRegistry.find_component<T>();

                if (component)
                {
                    sets.components.add(component);
                }
            }
        };

        (doAdd(static_cast<ComponentsOrTags*>(nullptr)), ...);

        return sets;
    }
}
