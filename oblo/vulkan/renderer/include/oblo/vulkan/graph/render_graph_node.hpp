#pragma once

#include <oblo/core/fixed_string.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>

namespace oblo::vk
{
    using render_graph_initialize = bool (*)(void*, void*);
    using render_graph_execute = void (*)(void*, void*);
    using render_graph_shutdown = void (*)(void*, void*);

    template <typename T, typename Context>
    concept is_compatible_render_graph_node = std::is_standard_layout_v<T> && std::is_trivially_destructible_v<T> &&
        std::is_default_constructible_v<T> && requires(T a, Context* c) { a.execute(c); };
}