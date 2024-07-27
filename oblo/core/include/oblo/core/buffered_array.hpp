#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/platform/compiler.hpp>
#include <oblo/core/stack_allocator.hpp>

namespace oblo
{
    template <typename T, usize N>
    class buffered_array : public dynamic_array<T>
    {
    public:
        using base = dynamic_array<T>;

    public:
        OBLO_FORCEINLINE buffered_array() : buffered_array{select_global_allocator<alignof(T)>()} {}

        OBLO_FORCEINLINE explicit buffered_array(allocator* fallback) : base{&m_allocator}, m_allocator{fallback}
        {
            base::reserve(N);
        }

        OBLO_FORCEINLINE buffered_array(const std::initializer_list<T> initializer) : buffered_array{}
        {
            static_cast<base&>(*this) = initializer;
        }

        OBLO_FORCEINLINE buffered_array(const buffered_array& other) : buffered_array{}
        {
            *this = other;
        }

        OBLO_FORCEINLINE buffered_array(const base& other) : buffered_array{}
        {
            *this = other;
        }

        OBLO_FORCEINLINE buffered_array(buffered_array&& other) noexcept : buffered_array{}
        {
            *this = std::move(other);
        }

        OBLO_FORCEINLINE buffered_array(base&& other) noexcept : buffered_array{}
        {
            *this = std::move(other);
        }

        OBLO_FORCEINLINE ~buffered_array()
        {
            // Make sure to reset the base when destroying, because of order of destruction we don't want references to
            // the derived
            *static_cast<base*>(this) = {};
        }

        OBLO_FORCEINLINE buffered_array& operator=(const base& other)
        {
            base::operator=(other);
            return *this;
        }

        OBLO_FORCEINLINE buffered_array& operator=(const buffered_array& other)
        {
            base::operator=(other);
            return *this;
        }

        OBLO_FORCEINLINE buffered_array& operator=(base&& other) noexcept
        {
            base::operator=(std::move(other));
            return *this;
        }

        OBLO_FORCEINLINE buffered_array& operator=(buffered_array&& other) noexcept
        {
            base::operator=(std::move(other));
            return *this;
        }

        void shrink_to_fit()
        {
            if (!m_allocator.is_stack_allocated(base::data_bytes()))
            {
                base::shrink_to_fit();
            }
        }

    private:
        stack_fallback_allocator<(sizeof(T) * N), alignof(T)> m_allocator;
    };
}