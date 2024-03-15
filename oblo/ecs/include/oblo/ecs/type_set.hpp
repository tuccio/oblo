#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/ecs/limits.hpp>
#include <oblo/ecs/traits.hpp>
#include <oblo/ecs/type_registry.hpp>

#include <compare>

namespace oblo::ecs
{
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

        void add(const type_set& other)
        {
            for (u32 i = 0; i < BlocksCount; ++i)
            {
                bitset[i] |= other.bitset[i];
            }
        }

        template <typename T>
        void remove(h32<T> index)
        {
            OBLO_ASSERT(index);
            const u64 maskOffset = index.value % BitsPerBlock;
            const u64 mask = u64(1) << maskOffset;
            const u64 block = index.value / BitsPerBlock;
            bitset[block] &= ~mask;
        }

        void remove(const type_set& other)
        {
            for (u32 i = 0; i < BlocksCount; ++i)
            {
                bitset[i] &= ~other.bitset[i];
            }
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

        [[nodiscard]] constexpr type_set intersection(const type_set& other) const
        {
            type_set r;

            for (u32 i = 0; i < BlocksCount; ++i)
            {
                r.bitset[i] = bitset[i] & other.bitset[i];
            }

            return r;
        }

        constexpr auto operator<=>(const type_set&) const = default;

        static constexpr u32 BitsPerBlock{64u};
        static constexpr u32 BlocksCount{round_up_div(MaxComponentTypes, BitsPerBlock)};

        u64 bitset[BlocksCount];
    };

    struct component_and_tag_sets
    {
        type_set components;
        type_set tags;

        constexpr auto operator<=>(const component_and_tag_sets&) const = default;
    };

    template <typename... ComponentsOrTags>
    component_and_tag_sets make_type_sets(const type_registry& typeRegistry)
    {
        component_and_tag_sets sets{};

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
