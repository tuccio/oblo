#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/math/power_of_two.hpp>

#include <initializer_list>
#include <memory>
#include <utility>

namespace oblo
{
    constexpr usize make_new_exponential_capacity(usize desired)
    {
        constexpr usize minReserve{16};
        return desired < minReserve ? minReserve : round_up_power_of_two(desired);
    }

    template <typename T>
    class dynamic_array
    {
    public:
        using value_type = T;

        using pointer = T*;
        using const_pointer = const T*;

        using reference = T&;
        using const_reference = const T&;

        using size_type = usize;
        using difference_type = ptrdiff;

        using iterator = T*;
        using const_iterator = const T*;

    public:
        dynamic_array();
        explicit dynamic_array(allocator* allocator);

        dynamic_array(const dynamic_array& other);
        dynamic_array(dynamic_array&& other) noexcept;

        dynamic_array& operator=(const dynamic_array& other);
        dynamic_array& operator=(dynamic_array&& other) noexcept;

        dynamic_array& operator=(std::initializer_list<T> values) noexcept;

        ~dynamic_array();

        void clear();

        void reserve(usize capacity);
        void reserve_exponential(usize capacity);

        void resize(usize size);
        void resize(usize size, const T& value);
        void resize_default(usize size);

        const T* cbegin() const;
        const T* cend() const;

        const T* begin() const;
        const T* end() const;

        T* begin();
        T* end();

        usize size() const;
        usize capacity() const;

        T& push_back(const T& e);
        T& push_back(T&& e);
        T& push_back_default();

        template <typename... Args>
        T& emplace_back(Args&&... args);

        void pop_back();

        T& front();
        const T& front() const;

        T& back();
        const T& back() const;

        void swap(dynamic_array& other);

        void shrink_to_fit() noexcept;

        const T& at(usize i) const;
        T& at(usize i);

        const T& operator[](usize i) const;
        T& operator[](usize i);

        T* data();
        const T* data() const;

        bool empty() const;

        iterator insert(const_iterator pos, const T& value);
        iterator insert(const_iterator pos, T&& value);

        iterator erase(const_iterator pos);
        iterator erase_unordered(const_iterator pos);

        template <typename Iterator>
        void assign(Iterator first, Iterator last) noexcept;

        template <typename Iterator>
        void assign(usize count, const T& value) noexcept;

        bool operator==(const dynamic_array& other) const noexcept;

    private:
        void maybe_grow_capacity(usize newCapacity, bool exact);
        void do_grow_capacity(usize newCapacity) noexcept;

    private:
        allocator* m_allocator{};
        T* m_data{};
        usize m_size{};
        usize m_capacity{};
    };

    template <typename T>
    dynamic_array<T>::dynamic_array() : m_allocator{select_global_allocator<alignof(T)>()}
    {
    }

    template <typename T>
    dynamic_array<T>::dynamic_array(allocator* allocator) : m_allocator{allocator}
    {
    }

    template <typename T>
    dynamic_array<T>::dynamic_array(const dynamic_array& other)
    {
        m_allocator = other.m_allocator;

        reserve(other.m_size);

        std::uninitialized_copy(other.begin(), other.end(), m_data);
        m_size = other.m_size;
    }

    template <typename T>
    dynamic_array<T>::dynamic_array(dynamic_array&& other) noexcept
    {
        m_allocator = other.m_allocator;
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;

        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    template <typename T>
    dynamic_array<T>& dynamic_array<T>::operator=(const dynamic_array& other)
    {
        clear();

        reserve(other.m_size);

        std::uninitialized_copy(other.begin(), other.end(), m_data);
        std::destroy(other.begin(), other.end());

        m_size = other.m_size;
        other.m_size = 0;

        return *this;
    }

    template <typename T>
    dynamic_array<T>& dynamic_array<T>::operator=(dynamic_array&& other) noexcept
    {
        clear();

        if (m_allocator == other.m_allocator)
        {
            swap(other);
        }
        else
        {
            reserve(other.m_size);

            std::uninitialized_move(other.begin(), other.end(), m_data);
            std::destroy(other.begin(), other.end());

            m_size = other.m_size;
            other.m_size = 0;
        }

        return *this;
    }

    template <typename T>
    inline dynamic_array<T>& dynamic_array<T>::operator=(std::initializer_list<T> values) noexcept
    {
        assign(values.begin(), values.end());
        return *this;
    }

    template <typename T>
    dynamic_array<T>::~dynamic_array()
    {
        clear();
        shrink_to_fit();
    }

    template <typename T>
    void dynamic_array<T>::clear()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            std::destroy(m_data, m_data + m_size);
        }

        m_size = 0;
    }

    template <typename T>
    void dynamic_array<T>::reserve(usize capacity)
    {
        maybe_grow_capacity(capacity, true);
    }

    template <typename T>
    void dynamic_array<T>::reserve_exponential(usize capacity)
    {
        maybe_grow_capacity(capacity, false);
    }

    template <typename T>
    void dynamic_array<T>::resize(usize newSize)
    {
        if (newSize > m_size)
        {
            maybe_grow_capacity(newSize, true);
            std::uninitialized_value_construct(m_data + m_size, m_data + newSize);
        }
        else if constexpr (!std::is_trivially_destructible_v<T>)
        {
            std::destroy(m_data + newSize, m_data + m_size);
        }

        m_size = newSize;
    }

    template <typename T>
    void dynamic_array<T>::resize(usize newSize, const T& value)
    {
        if (newSize > m_size)
        {
            maybe_grow_capacity(newSize, true);
            std::uninitialized_fill(m_data + m_size, m_data + newSize, value);
        }
        else
        {
            std::destroy(m_data + newSize, m_data + m_size);
        }

        m_size = newSize;
    }

    template <typename T>
    void dynamic_array<T>::resize_default(usize newSize)
    {
        if (newSize > m_size)
        {
            maybe_grow_capacity(newSize, true);
            std::uninitialized_default_construct(m_data + m_size, m_data + newSize);
        }
        else
        {
            std::destroy(m_data + newSize, m_data + m_size);
        }

        m_size = newSize;
    }

    template <typename T>
    const T* dynamic_array<T>::cbegin() const
    {
        return m_data;
    }

    template <typename T>
    const T* dynamic_array<T>::cend() const
    {
        return m_data + m_size;
    }

    template <typename T>
    const T* dynamic_array<T>::begin() const
    {
        return m_data;
    }

    template <typename T>
    const T* dynamic_array<T>::end() const
    {
        return m_data + m_size;
    }

    template <typename T>
    T* dynamic_array<T>::begin()
    {
        return m_data;
    }

    template <typename T>
    T* dynamic_array<T>::end()
    {
        return m_data + m_size;
    }

    template <typename T>
    usize dynamic_array<T>::size() const
    {
        return m_size;
    }

    template <typename T>
    usize dynamic_array<T>::capacity() const
    {
        return m_capacity;
    }

    template <typename T>
    T& dynamic_array<T>::push_back(const T& e)
    {
        maybe_grow_capacity(m_size + 1, false);
        auto* const r = new (m_data + m_size) T(e);
        ++m_size;
        return *r;
    }

    template <typename T>
    T& dynamic_array<T>::push_back(T&& e)
    {
        maybe_grow_capacity(m_size + 1, false);
        auto* const r = new (m_data + m_size) T(std::move(e));
        ++m_size;
        return *r;
    }

    template <typename T>
    T& dynamic_array<T>::push_back_default()
    {
        maybe_grow_capacity(m_size + 1, false);
        auto* const r = new (m_data + m_size) T;
        ++m_size;
        return *r;
    }

    template <typename T>
    template <typename... Args>
    T& dynamic_array<T>::emplace_back(Args&&... args)
    {
        maybe_grow_capacity(m_size + 1, false);
        auto* const r = new (m_data + m_size) T(std::forward<Args>(args)...);
        ++m_size;
        return *r;
    }

    template <typename T>
    template <typename Iterator>
    void dynamic_array<T>::assign(Iterator first, Iterator last) noexcept
    {
        clear();

        // TODO: Should we check if it is contiguous?
        const auto count = last - first;

        reserve(count);

        for (T* it = m_data; it != m_data + count; ++it, ++first)
        {
            new (it) T(*first);
        }

        m_size = count;
    }

    template <typename T>
    template <typename Iterator>
    inline void dynamic_array<T>::assign(usize count, const T& value) noexcept
    {
        clear();

        reserve(count);

        for (T* it = m_data; it != m_data + count; ++it)
        {
            new (it) T(value);
        }

        m_size = count;
    }

    template <typename T>
    void dynamic_array<T>::pop_back()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            back().~T();
        }

        --m_size;
    }

    template <typename T>
    T& dynamic_array<T>::front()
    {
        OBLO_ASSERT(m_size > 0);
        return *m_data;
    }

    template <typename T>
    const T& dynamic_array<T>::front() const
    {
        OBLO_ASSERT(m_size > 0);
        return *m_data;
    }

    template <typename T>
    T& dynamic_array<T>::back()
    {
        OBLO_ASSERT(m_size > 0);
        return *(m_data + m_size - 1);
    }

    template <typename T>
    const T& dynamic_array<T>::back() const
    {
        OBLO_ASSERT(m_size > 0);
        return *(m_data + m_size - 1);
    }

    template <typename T>
    void dynamic_array<T>::swap(dynamic_array& other)
    {
        OBLO_ASSERT(m_allocator == other.m_allocator, "Swapping is only possible if the allocator is the same");

        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    template <typename T>
    void dynamic_array<T>::shrink_to_fit() noexcept
    {
        if (m_size != m_capacity)
        {
            if (m_size == 0)
            {
                if (m_data)
                {
                    m_allocator->deallocate(reinterpret_cast<byte*>(m_data), m_capacity * sizeof(T), alignof(T));
                }

                m_data = nullptr;
            }
            else
            {
                dynamic_array tmp{m_allocator};
                tmp.reserve(m_size);

                std::uninitialized_move(begin(), end(), tmp.m_data);
                tmp.m_size = m_size;

                swap(tmp);
            }
        }
    }

    template <typename T>
    const T& dynamic_array<T>::at(usize i) const
    {
        OBLO_ASSERT(i < m_size);
        return *(m_data + i);
    }

    template <typename T>
    T& dynamic_array<T>::at(usize i)
    {
        OBLO_ASSERT(i < m_size);
        return *(m_data + i);
    }

    template <typename T>
    const T& dynamic_array<T>::operator[](usize i) const
    {
        OBLO_ASSERT(i < m_size);
        return *(m_data + i);
    }

    template <typename T>
    T& dynamic_array<T>::operator[](usize i)
    {
        OBLO_ASSERT(i < m_size);
        return *(m_data + i);
    }

    template <typename T>
    T* dynamic_array<T>::data()
    {
        return m_data;
    }

    template <typename T>
    const T* dynamic_array<T>::data() const
    {
        return m_data;
    }

    template <typename T>
    bool dynamic_array<T>::empty() const
    {
        return m_size != 0;
    }

    template <typename T>
    void dynamic_array<T>::maybe_grow_capacity(usize newCapacity, bool exact)
    {
        if (newCapacity > m_capacity)
        {
            do_grow_capacity(exact ? newCapacity : make_new_exponential_capacity(newCapacity));
        }
    }

    template <typename T>
    void dynamic_array<T>::do_grow_capacity(usize newCapacity) noexcept
    {
        byte* const newData = m_allocator->allocate(newCapacity * sizeof(T), alignof(T));

        if (m_size != 0)
        {
            auto* const b = begin();
            auto* const e = end();

            std::uninitialized_move(b, e, reinterpret_cast<T*>(newData));
            std::destroy(b, e);
        }

        if (m_data)
        {
            m_allocator->deallocate(reinterpret_cast<byte*>(m_data), m_capacity * sizeof(T), alignof(T));
        }

        m_data = reinterpret_cast<T*>(newData);
        m_capacity = newCapacity;
    }

    template <typename T>
    bool dynamic_array<T>::operator==(const dynamic_array& other) const noexcept
    {
        const auto isSizeEqual = m_size == other.m_size;
        return isSizeEqual && std::equal(begin(), end(), other.begin());
    }
}