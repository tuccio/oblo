#pragma once

#include <oblo/core/fixed_string.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>

namespace oblo
{
    using render_graph_initialize = bool (*)(void*, void*);
    using render_graph_execute = void (*)(void*, void*);
    using render_graph_shutdown = void (*)(void*, void*);

    template <typename T, typename Context>
    concept is_compatible_render_graph_node = std::is_standard_layout_v<T> && std::is_trivially_destructible_v<T> &&
        std::is_default_constructible_v<T> && requires(T a, Context* c)
    {
        a.execute(c);
    };

    template <typename T>
    concept is_compatible_render_graph_pin =
        std::is_trivially_destructible_v<T> && std::is_trivially_default_constructible_v<T>;

    template <typename T, fixed_string Name>
    requires is_compatible_render_graph_pin<T>
    struct render_node_in
    {
        static constexpr std::string_view name()
        {
            return std::string_view{Name.string};
        }

        const T* data;
    };

    template <typename T, fixed_string Name>
    requires is_compatible_render_graph_pin<T>
    struct render_node_out
    {
        static constexpr std::string_view name()
        {
            return std::string_view{Name.string};
        }

        T* operator->() const noexcept
        {
            return data;
        }

        T& operator*() const noexcept
        {
            return data;
        }

        T* data;
    };
}