#pragma once

namespace oblo
{
    namespace detail
    {
        template <typename T>
        struct meta_impl
        {
            static void key() {}
        };
    }

    /// @brief Uniquely identifies a type.
    /// The underlying type will be the same for each T, so it can be used in arrays.
    /// It may be used for type equality checks, although the check breaks across DLL boundaries.
    /// Hence it should not be stored but just used within a single function.
    template <typename T>
    inline constexpr auto meta_id = detail::meta_impl<T>::key;
}