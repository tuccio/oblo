#pragma once

namespace oblo
{
    class allocator;
    class cstring_view;
    class hashed_string_view;
    class string;
    class string_builder;
    class string_interner;
    class string_view;

    template <typename T>
    class deque;

    template <typename T>
    class dynamic_array;

    template <typename T>
    struct hash;

    template <typename>
    class function_ref;

    template <typename>
    class future;

    template <typename>
    class promise;

    template <typename T, typename D>
    class unique_ptr;
}