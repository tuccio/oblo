#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <compare>
#include <initializer_list>
#include <type_traits>

namespace oblo
{
    struct deque_config
    {
        /// @brief The number of elements per deque chunk, it must be a positive power of two.
        usize elementsPerChunk;
    };

    template <typename T>
    class deque_iterator;

    template <typename T>
    class deque
    {
    public:
        using value_type = T;

        using pointer = T*;
        using const_pointer = const T*;

        using reference = T&;
        using const_reference = const T&;

        using size_type = usize;
        using difference_type = ptrdiff;

        using iterator = deque_iterator<T>;
        using const_iterator = deque_iterator<const T>;

    public:
        deque();
        explicit deque(allocator* allocator);
        explicit deque(allocator* allocator, std::initializer_list<T> init);
        explicit deque(allocator* allocator, usize count);
        explicit deque(allocator* allocator, deque_config cfg);
        explicit deque(allocator* allocator, deque_config cfg, usize count);

        deque(const deque& other);
        deque(deque&& other) noexcept;

        deque& operator=(const deque& other);
        deque& operator=(deque&& other) noexcept;

        deque& operator=(std::initializer_list<T> values) noexcept;

        ~deque();

        void clear();

        void reserve(usize capacity);

        void resize(usize size);
        void resize(usize size, const T& value);
        void resize_default(usize size);

        const_iterator cbegin() const;
        const_iterator cend() const;

        const_iterator begin() const;
        const_iterator end() const;

        iterator begin();
        iterator end();

        usize size() const;
        usize capacity() const;
        usize elements_per_chunk() const;

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

        void swap(deque& other);

        void shrink_to_fit() noexcept;

        const T& at(usize i) const;
        T& at(usize i);

        const T& operator[](usize i) const;
        T& operator[](usize i);

        bool empty() const;

        iterator insert(const_iterator pos, const T& value);
        iterator insert(const_iterator pos, T&& value);

        template <typename OtherIt>
        iterator insert(const_iterator pos, OtherIt begin, OtherIt end);

        iterator erase(const_iterator pos);
        iterator erase(const_iterator begin, const_iterator end);
        iterator erase_unordered(const_iterator pos);

        template <typename OtherIt>
        iterator append(OtherIt begin, OtherIt end);

        template <typename Iterator>
        void assign(Iterator first, Iterator last) noexcept;

        void assign(usize count, const T& value) noexcept;
        void assign_default(usize count) noexcept;

        bool operator==(const deque& other) const noexcept;

        template <typename Other>
            requires(!std::convertible_to<const Other&, const deque<T>&>)
        bool operator==(const Other& other) const noexcept;

        allocator* get_allocator() const noexcept;

    private:
        using chunk = T*;

    private:
        template <typename F>
        void resize_internal(usize size, F&& init);

        void shrink_internal(usize size);

        void free_empty(usize firstFreeChunk = 0);

        OBLO_FORCEINLINE T& at_unsafe(usize index);
        OBLO_FORCEINLINE const T& at_unsafe(usize index) const;

        static constexpr usize get_default_elements_per_chunk();

    private:
        dynamic_array<chunk> m_chunks;
        usize m_size{};
        usize m_elementsPerChunk{};
    };

    template <typename T>
    class deque_iterator
    {
    public:
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        using size_type = usize;
        using difference_type = ptrdiff;

    public:
        deque_iterator() = default;
        deque_iterator(const deque_iterator&) = default;

        deque_iterator(T* const* chunks, usize elementsPerChunk, usize index) :
            m_chunks{chunks}, m_elementsPerChunk{elementsPerChunk}, m_index{index}
        {
        }

        deque_iterator& operator=(const deque_iterator&) = default;

        OBLO_FORCEINLINE constexpr decltype(auto) operator*() const
        {
            return m_chunks[m_index / m_elementsPerChunk][m_index & (m_elementsPerChunk - 1)];
        }

        OBLO_FORCEINLINE constexpr decltype(auto) operator->() const
        {
            return &m_chunks[m_index / m_elementsPerChunk][m_index & (m_elementsPerChunk - 1)];
        }

        OBLO_FORCEINLINE constexpr deque_iterator& operator++()
        {
            ++m_index;
            return *this;
        }

        OBLO_FORCEINLINE constexpr deque_iterator operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        OBLO_FORCEINLINE constexpr deque_iterator& operator--()
        {
            OBLO_ASSERT(m_index > 0);
            --m_index;
            return *this;
        }

        OBLO_FORCEINLINE constexpr deque_iterator operator--(int)
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        OBLO_FORCEINLINE auto operator-(const deque_iterator& other)
        {
            return other.m_index - m_index;
        }

        OBLO_FORCEINLINE constexpr auto operator<=>(const deque_iterator& other) const
        {
            return m_index <=> other.m_index;
        }

        OBLO_FORCEINLINE constexpr auto operator!=(const deque_iterator& other) const
        {
            return m_index != other.m_index;
        }

    private:
        T* const* m_chunks{};
        usize m_elementsPerChunk{};
        usize m_index{};
    };

    template <typename T>
    constexpr usize deque<T>::get_default_elements_per_chunk()
    {
        return max<usize>(16u, 4096u / sizeof(T));
    }

    template <typename T>
    deque<T>::deque() : m_elementsPerChunk{get_default_elements_per_chunk()}
    {
    }

    template <typename T>
    deque<T>::deque(allocator* allocator) : m_chunks{allocator}, m_elementsPerChunk{get_default_elements_per_chunk()}
    {
    }

    template <typename T>
    deque<T>::deque(allocator* allocator, std::initializer_list<T> init) : deque{allocator}
    {
        *this = init;
    }

    template <typename T>
    deque<T>::deque(allocator* allocator, deque_config cfg) :
        m_chunks{allocator}, m_elementsPerChunk{cfg.elementsPerChunk}
    {
    }

    template <typename T>
    deque<T>::deque(allocator* allocator, deque_config cfg, usize count) :
        m_chunks{allocator}, m_elementsPerChunk{cfg.elementsPerChunk}
    {
        resize(count);
    }

    // template <typename T>
    // deque<T>::deque(allocator* allocator, usize count) : deque{allocator}
    //{
    //     if (count != 0)
    //     {
    //         // TODO
    //         // byte* const newData = m_allocator->allocate(count * sizeof(T), alignof(T));

    //        m_data = reinterpret_cast<T*>(newData);
    //        m_capacity = count;
    //        m_size = count;

    //        std::uninitialized_value_construct(m_data, m_data + count);
    //    }
    //}

    template <typename T>
    deque<T>::deque(const deque& other) : m_chunks{other.get_allocator()}, m_elementsPerChunk{other.m_elementsPerChunk}
    {
        reserve(other.m_size);

        for (usize i = 0; i < other.m_size / m_elementsPerChunk; ++i)
        {
            const T* const src = other.m_chunks[i];
            T* const dst = m_chunks[i];
            std::uninitialized_copy(src, src + m_elementsPerChunk, dst);
        }

        {
            const usize elementsInChunk = other.m_size & (m_elementsPerChunk - 1);
            const T* const src = other.m_chunks.back();
            T* const dst = m_chunks.back();
            std::uninitialized_copy(src, src + elementsInChunk, dst);
        }

        m_size = other.m_size;
    }

    template <typename T>
    deque<T>::deque(deque&& other) noexcept :
        m_chunks{std::move(other.m_chunks)}, m_elementsPerChunk{other.m_elementsPerChunk}, m_size{other.m_size}
    {
        other.m_size = 0;
    }

    template <typename T>
    deque<T>& deque<T>::operator=(const deque& other)
    {
        clear();

        resize_internal(other.m_size,
            [it = other.begin()](T* b, T* e) mutable
            {
                for (T* d = b; d != e; ++it, ++d)
                {
                    new (d) T{*it};
                }
            });

        return *this;
    }

    template <typename T>
    deque<T>& deque<T>::operator=(deque&& other) noexcept
    {
        clear();

        if (get_allocator() == other.get_allocator())
        {
            swap(other);
        }
        else
        {
            clear();

            resize_internal(other.m_size,
                [it = other.begin()](T* b, T* e) mutable
                {
                    for (T* d = b; d != e; ++it, ++d)
                    {
                        new (d) T{std::move(*it)};
                    }
                });

            return *this;
        }

        return *this;
    }

    template <typename T>
    inline deque<T>& deque<T>::operator=(std::initializer_list<T> values) noexcept
    {
        assign(values.begin(), values.end());
        return *this;
    }

    template <typename T>
    deque<T>::~deque()
    {
        clear();
        free_empty();
    }

    template <typename T>
    void deque<T>::clear()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            usize chunkIndex = 0;

            while (m_size > 0)
            {
                T* const chunk = m_chunks[chunkIndex];
                ++chunkIndex;

                const auto elements = min(m_size, m_elementsPerChunk);

                std::destroy(chunk, chunk + elements);

                m_size -= elements;
            }
        }
        else
        {
            m_size = 0;
        }
    }

    template <typename T>
    void deque<T>::reserve(usize capacity)
    {
        const auto chunksCount = round_up_div(capacity, m_elementsPerChunk);
        const auto oldSize = m_chunks.size();

        if (chunksCount > oldSize)
        {
            m_chunks.reserve_exponential(chunksCount);
            m_chunks.resize_default(chunksCount);

            allocator* const a = m_chunks.get_allocator();

            for (usize i = oldSize; i < chunksCount; ++i)
            {
                chunk const c = reinterpret_cast<chunk>(a->allocate(sizeof(T) * m_elementsPerChunk, alignof(T)));
                m_chunks[i] = c;
            }
        }
    }

    template <typename T>
    template <typename F>
    void deque<T>::resize_internal(usize newSize, F&& init)
    {
        if (newSize > m_size)
        {
            reserve(newSize);

            const usize firstElement = m_size + 1;
            const usize firstChunk = firstElement / m_elementsPerChunk;

            // Deal with the first chunk, where we might need to apply an offset
            {
                T* const chunk = m_chunks[firstChunk];

                const usize elementsInFirstChunk = m_size & (m_elementsPerChunk - 1);
                const auto elements = min(newSize - m_size, m_elementsPerChunk - elementsInFirstChunk);

                T* const begin = chunk + elementsInFirstChunk;
                init(begin, begin + elements);

                m_size += elements;
            }

            for (usize chunkIndex = firstChunk + 1; m_size < newSize; ++chunkIndex)
            {
                T* const chunk = m_chunks[chunkIndex];
                const auto elements = min(newSize - m_size, m_elementsPerChunk);
                m_size += elements;

                init(chunk, chunk + elements);
            }
        }
        else
        {
            shrink_internal(newSize);
        }
    }

    template <typename T>
    void deque<T>::resize(usize newSize)
    {
        resize_internal(newSize, [](T* first, T* last) { std::uninitialized_value_construct(first, last); });
    }

    template <typename T>
    void deque<T>::resize(usize newSize, const T& value)
    {
        resize_internal(newSize, [&value](T* first, T* last) { std::uninitialized_fill(first, last, value); });
    }

    template <typename T>
    void deque<T>::resize_default(usize newSize)
    {
        resize_internal(newSize, [](T* first, T* last) { std::uninitialized_default_construct(first, last); });
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::const_iterator deque<T>::cbegin() const
    {
        return const_iterator{m_chunks.data(), m_elementsPerChunk, 0};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::const_iterator deque<T>::cend() const
    {
        return const_iterator{m_chunks.data(), m_elementsPerChunk, m_size};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::const_iterator deque<T>::begin() const
    {
        return const_iterator{m_chunks.data(), m_elementsPerChunk, 0};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::const_iterator deque<T>::end() const
    {
        return const_iterator{m_chunks.data(), m_elementsPerChunk, m_size};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::iterator deque<T>::begin()
    {
        return iterator{m_chunks.data(), m_elementsPerChunk, 0};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::iterator deque<T>::end()
    {
        return iterator{m_chunks.data(), m_elementsPerChunk, m_size};
    }

    template <typename T>
    OBLO_FORCEINLINE usize deque<T>::size() const
    {
        return m_size;
    }

    template <typename T>
    OBLO_FORCEINLINE usize deque<T>::capacity() const
    {
        return m_elementsPerChunk * m_chunks.size();
    }

    template <typename T>
    OBLO_FORCEINLINE usize deque<T>::elements_per_chunk() const
    {
        return m_elementsPerChunk;
    }

    template <typename T>
    T& deque<T>::push_back(const T& e)
    {
        reserve(m_size + 1);
        T* const ptr = &at_unsafe(m_size);
        new (ptr) T(e);
        ++m_size;
        return *ptr;
    }

    template <typename T>
    T& deque<T>::push_back(T&& e)
    {
        reserve(m_size + 1);
        T* const ptr = &at_unsafe(m_size);
        new (ptr) T(std::move(e));
        ++m_size;
        return *ptr;
    }

    template <typename T>
    T& deque<T>::push_back_default()
    {
        reserve(m_size + 1);
        T* const ptr = &at_unsafe(m_size);
        new (ptr) T;
        ++m_size;
        return *ptr;
    }

    template <typename T>
    template <typename... Args>
    T& deque<T>::emplace_back(Args&&... args)
    {
        reserve(m_size + 1);
        T* const ptr = &at_unsafe(m_size);
        new (ptr) T(std::forward<Args>(args)...);
        ++m_size;
        return *ptr;
    }

    template <typename T>
    inline deque<T>::iterator deque<T>::insert(const_iterator pos, const T& value)
    {
        OBLO_ASSERT(pos <= end());

        const auto b = begin();
        const auto i = pos - b;

        emplace_back(value);

        const auto it = b + i;

        rotate(it, b + (m_size - 1), b + m_size);
        return it;
    }

    template <typename T>
    inline deque<T>::iterator deque<T>::insert(const_iterator pos, T&& value)
    {
        OBLO_ASSERT(pos <= end());

        const auto b = begin();
        const auto i = pos - b;

        emplace_back(std::move(value));

        const auto it = b + i;

        rotate(it, b + (m_size - 1), b + m_size);
        return it;
    }

    // template <typename T>
    // inline deque<T>::iterator deque<T>::erase(const_iterator pos)
    //{
    //     return erase(pos, pos + 1);
    // }

    // template <typename T>
    // inline deque<T>::iterator deque<T>::erase(const_iterator begin, const_iterator end)
    //{
    //     OBLO_ASSERT(begin < m_data + m_size);

    //    const auto beginIt = const_cast<iterator>(begin);
    //    const auto endIt = const_cast<iterator>(end);

    //    rotate(beginIt, endIt, m_data + m_size);

    //    const auto newSize = m_size - (end - begin);
    //    resize(newSize);

    //    return beginIt;
    //}

    // template <typename T>
    // inline deque<T>::iterator deque<T>::erase_unordered(const_iterator pos)
    //{
    //     OBLO_ASSERT(pos < m_data + m_size);

    //    const auto it = const_cast<iterator>(pos);
    //    const auto backIt = m_data + m_size - 1;

    //    if (it != backIt)
    //    {
    //        std::swap(*it, *backIt);
    //    }

    //    pop_back();

    //    return it;
    //}

    // template <typename T>
    // template <typename OtherIt>
    // inline deque<T>::iterator deque<T>::insert(const_iterator pos, OtherIt it, OtherIt end)
    //{
    //     const auto i = pos - m_data;
    //     OBLO_ASSERT(pos <= m_data + m_size);

    //    // If we insert at the end, it's just an append, otherwise it needs a rotate
    //    const auto inTheMiddle = pos != m_data + m_size;

    //    const auto appendedIt = append(it, end);
    //    const auto insertedIt = m_data + i;

    //    if (inTheMiddle)
    //    {
    //        rotate(insertedIt, appendedIt, m_data + m_size);
    //    }

    //    return insertedIt;
    //}

    template <typename T>
    template <typename OtherIt>
    inline deque<T>::iterator deque<T>::append(OtherIt it, OtherIt end)
    {
        const auto count = end - it;

        const auto first = m_size;
        reserve_exponential(m_size + count);

        for (; it != end; ++it)
        {
            emplace_back(*it);
        }

        return begin() + first;
    }

    template <typename T>
    template <typename Iterator>
    void deque<T>::assign(Iterator first, Iterator last) noexcept
    {
        clear();

        const auto count = last - first;

        resize_internal(count,
            [it = first, &last](T* b, T* e) mutable
            {
                for (T* d = b; d != e; ++d, ++it)
                {
                    new (d) T{*it};
                }
            });
    }

    template <typename T>
    inline void deque<T>::assign(usize count, const T& value) noexcept
    {
        clear();
        resize(count, value);
    }

    template <typename T>
    inline void deque<T>::assign_default(usize count) noexcept
    {
        clear();
        resize_default(count);
    }

    // template <typename T>
    // void deque<T>::pop_back()
    //{
    //     if constexpr (!std::is_trivially_destructible_v<T>)
    //     {
    //         back().~T();
    //     }

    //    --m_size;
    //}

    template <typename T>
    T& deque<T>::front()
    {
        OBLO_ASSERT(m_size > 0);
        return m_chunks[0][0];
    }

    template <typename T>
    const T& deque<T>::front() const
    {
        OBLO_ASSERT(m_size > 0);
        return m_chunks[0][0];
    }

    template <typename T>
    T& deque<T>::back()
    {
        OBLO_ASSERT(m_size > 0);
        return at_unsafe(m_size - 1);
    }

    template <typename T>
    const T& deque<T>::back() const
    {
        OBLO_ASSERT(m_size > 0);
        return at_unsafe(m_size - 1);
    }

    template <typename T>
    void deque<T>::swap(deque& other)
    {
        OBLO_ASSERT(get_allocator() == other.get_allocator(), "Swapping is only possible if the allocator is the same");

        std::swap(m_chunks, other.m_chunks);
        std::swap(m_size, other.m_size);
        std::swap(m_elementsPerChunk, other.m_elementsPerChunk);
    }

    template <typename T>
    void deque<T>::shrink_to_fit() noexcept
    {
        const usize requiredChunks = round_up_div(m_size, m_elementsPerChunk);

        if (requiredChunks != m_chunks.size())
        {
            shrink_internal(m_size);
            free_empty(requiredChunks + 1);
            m_chunks.resize(requiredChunks);
            m_chunks.shrink_to_fit();
        }
    }

    template <typename T>
    const T& deque<T>::at(usize i) const
    {
        OBLO_ASSERT(i < m_size);
        return at_unsafe(i);
    }

    template <typename T>
    T& deque<T>::at(usize i)
    {
        OBLO_ASSERT(i < m_size);
        return at_unsafe(i);
    }

    template <typename T>
    const T& deque<T>::operator[](usize i) const
    {
        OBLO_ASSERT(i < m_size);
        return at_unsafe(i);
    }

    template <typename T>
    T& deque<T>::operator[](usize i)
    {
        OBLO_ASSERT(i < m_size);
        return at_unsafe(i);
    }

    template <typename T>
    OBLO_FORCEINLINE bool deque<T>::empty() const
    {
        return m_size == 0;
    }

    template <typename T>
    bool deque<T>::operator==(const deque& other) const noexcept
    {
        const auto isSizeEqual = m_size == other.m_size;
        return isSizeEqual && std::equal(begin(), end(), other.begin());
    }

    template <typename T>
    void deque<T>::shrink_internal(usize newSize)
    {
        if constexpr (std::is_trivially_destructible_v<T>)
        {
            m_size = newSize;
        }
        else
        {
            const usize firstElement = newSize + 1;
            const usize firstChunk = firstElement / m_elementsPerChunk;

            // Deal with the first chunk, where we might need to apply an offset
            {
                T* const chunk = m_chunks[firstChunk];

                const usize elementsInFirstChunk = m_size & (m_elementsPerChunk - 1);
                const auto elements = min(m_size - newSize, m_elementsPerChunk - elementsInFirstChunk);

                T* const begin = chunk + elementsInFirstChunk;
                std::destroy(begin, begin + elements);

                m_size -= elements;
            }

            for (usize chunkIndex = firstChunk + 1; newSize < m_size; ++chunkIndex)
            {
                T* const chunk = m_chunks[chunkIndex];
                const auto elements = min(m_size - newSize, m_elementsPerChunk);
                m_size -= elements;

                std::destroy(chunk, chunk + elements);
            }
        }
    }

    template <typename T>
    inline void deque<T>::free_empty(usize firstFreeChunk)
    {
        allocator* const a = m_chunks.get_allocator();

        const usize chunkSize = sizeof(T) * m_elementsPerChunk;

        for (auto it = m_chunks.begin() + firstFreeChunk; it != m_chunks.end(); ++it)
        {
            const chunk c = *it;
            OBLO_ASSERT(c);
            a->deallocate(reinterpret_cast<byte*>(c), chunkSize, alignof(T));
        }
    }

    template <typename T>
    OBLO_FORCEINLINE T& deque<T>::at_unsafe(usize index)
    {
        T* const chunk = m_chunks[index / m_elementsPerChunk];
        return chunk[index & (m_elementsPerChunk - 1)];
    }

    template <typename T>
    OBLO_FORCEINLINE const T& deque<T>::at_unsafe(usize index) const
    {
        const T* const chunk = m_chunks[index / m_elementsPerChunk];
        return chunk[index & (m_elementsPerChunk - 1)];
    }

    template <typename T>
    template <typename Other>
        requires(!std::convertible_to<const Other&, const deque<T>&>)
    bool deque<T>::operator==(const Other& other) const noexcept
    {
        return std::equal(begin(), end(), std::begin(other), std::end(other));
    }

    template <typename T>
    OBLO_FORCEINLINE allocator* deque<T>::get_allocator() const noexcept
    {
        return m_chunks.get_allocator();
    }
}