#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/rotate.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <compare>
#include <initializer_list>
#include <iterator>
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

        T& push_front(const T& e);
        T& push_front(T&& e);
        T& push_front_default();

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

        void grow_front(usize chunks);

        OBLO_FORCEINLINE T& at_unsafe(usize index);
        OBLO_FORCEINLINE const T& at_unsafe(usize index) const;

        static constexpr usize get_default_elements_per_chunk();

    private:
        dynamic_array<chunk> m_chunks;
        usize m_start{};
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

        // using iterator_category = std::forward_iterator_tag;

    public:
        deque_iterator() = default;
        deque_iterator(const deque_iterator&) = default;

        template <typename U>
            requires std::is_same_v<T, const U>
        deque_iterator(const deque_iterator<U>& o) :
            m_chunks{o.m_chunks}, m_elementsPerChunk{o.m_elementsPerChunk}, m_index{o.m_index}
        {
        }

        deque_iterator(T* const* chunks, usize elementsPerChunk, usize index) :
            m_chunks{chunks}, m_elementsPerChunk{elementsPerChunk}, m_index{index}
        {
        }

        deque_iterator& operator=(const deque_iterator&) = default;

        OBLO_FORCEINLINE constexpr T& operator*() const
        {
            return m_chunks[m_index / m_elementsPerChunk][m_index & (m_elementsPerChunk - 1)];
        }

        OBLO_FORCEINLINE constexpr T* operator->() const
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

        OBLO_FORCEINLINE constexpr deque_iterator operator+(size_type offset) const
        {
            auto tmp = *this;
            tmp.m_index += offset;
            return tmp;
        }

        OBLO_FORCEINLINE constexpr deque_iterator operator-(size_type offset) const
        {
            OBLO_ASSERT(m_index >= offset);

            auto tmp = *this;
            tmp.m_index -= offset;
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

        OBLO_FORCEINLINE difference_type operator-(const deque_iterator& other) const
        {
            return m_index - other.m_index;
        }

        OBLO_FORCEINLINE constexpr auto operator<=>(const deque_iterator& other) const
        {
            OBLO_ASSERT(m_chunks == other.m_chunks);
            return m_index <=> other.m_index;
        }

        OBLO_FORCEINLINE constexpr auto operator==(const deque_iterator& other) const
        {
            OBLO_ASSERT(m_chunks == other.m_chunks);
            return m_index == other.m_index;
        }

        OBLO_FORCEINLINE constexpr auto operator!=(const deque_iterator& other) const
        {
            OBLO_ASSERT(m_chunks == other.m_chunks);
            return m_index != other.m_index;
        }

    private:
        template <typename U>
        friend class deque;

        template <typename U>
        friend class deque_iterator;

    private:
        auto remove_const() const
        {
            using U = std::remove_const_t<T>;
            return deque_iterator<U>{const_cast<U* const*>(m_chunks), m_elementsPerChunk, m_index};
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
    deque<T>::deque() :
        m_chunks{select_global_allocator<max(alignof(T), alignof(chunk))>()},
        m_elementsPerChunk{get_default_elements_per_chunk()}
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
        const auto chunksCount = round_up_div(m_start + capacity, m_elementsPerChunk);
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

            const usize lastOldElement = m_start + m_size;
            const usize firstNewElement = lastOldElement + 1;
            const usize firstChunk = firstNewElement / m_elementsPerChunk;

            // Deal with the first chunk, where we might need to apply an offset
            {
                T* const chunk = m_chunks[firstChunk];

                const usize elementsInFirstChunk = lastOldElement & (m_elementsPerChunk - 1);
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
        return const_iterator{m_chunks.data(), m_elementsPerChunk, m_start};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::const_iterator deque<T>::cend() const
    {
        return const_iterator{m_chunks.data(), m_elementsPerChunk, m_start + m_size};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::const_iterator deque<T>::begin() const
    {
        return const_iterator{m_chunks.data(), m_elementsPerChunk, m_start};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::const_iterator deque<T>::end() const
    {
        return const_iterator{m_chunks.data(), m_elementsPerChunk, m_start + m_size};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::iterator deque<T>::begin()
    {
        return iterator{m_chunks.data(), m_elementsPerChunk, m_start};
    }

    template <typename T>
    OBLO_FORCEINLINE deque<T>::iterator deque<T>::end()
    {
        return iterator{m_chunks.data(), m_elementsPerChunk, m_start + m_size};
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
    T& deque<T>::push_front(const T& e)
    {
        if (m_start == 0)
        {
            grow_front(1);
        }

        --m_start;

        T* const ptr = &at_unsafe(0);
        new (ptr) T(e);
        ++m_size;
        return *ptr;
    }

    template <typename T>
    T& deque<T>::push_front(T&& e)
    {
        if (m_start == 0)
        {
            grow_front(1);
        }

        --m_start;

        T* const ptr = &at_unsafe(0);
        new (ptr) T(std::move(e));
        ++m_size;
        return *ptr;
    }

    template <typename T>
    T& deque<T>::push_front_default()
    {
        if (m_start == 0)
        {
            grow_front(1);
        }

        --m_start;

        T* const ptr = &at_unsafe(0);
        new (ptr) T;
        ++m_size;
        return *ptr;
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

        rotate(it, b + (m_start + m_size - 1), end());
        return it;
    }

    template <typename T>
    deque<T>::iterator deque<T>::erase(const_iterator pos)
    {
        return erase(pos, pos + 1);
    }

    template <typename T>
    deque<T>::iterator deque<T>::erase(const_iterator begin, const_iterator end)
    {
        OBLO_ASSERT(begin < this->end());

        const auto erasedCount = end - begin;
        const auto beginOffset = begin.m_index;

        rotate(begin.remove_const(), end.remove_const(), this->end());

        const auto newSize = m_size - erasedCount;
        shrink_internal(newSize);

        return this->begin() + beginOffset;
    }

    template <typename T>
    deque<T>::iterator deque<T>::erase_unordered(const_iterator pos)
    {
        OBLO_ASSERT(pos < end());

        const auto it = pos.remove_const();
        const auto backIt = end() - 1;

        if (it != backIt)
        {
            std::swap(*it, *backIt);
        }

        pop_back();

        return it;
    }

    template <typename T>
    template <typename OtherIt>
    inline deque<T>::iterator deque<T>::insert(const_iterator pos, OtherIt it, OtherIt last)
    {
        const auto i = pos - begin();
        OBLO_ASSERT(pos <= end());

        // If we insert at the end, it's just an append, otherwise it needs a rotate
        const auto inTheMiddle = pos != end();

        const auto appendedIt = append(it, last);
        const auto insertedIt = begin() + i;

        if (inTheMiddle)
        {
            rotate(insertedIt, appendedIt, end());
        }

        return insertedIt;
    }

    template <typename T>
    template <typename OtherIt>
    inline deque<T>::iterator deque<T>::append(OtherIt first, OtherIt last)
    {
        const auto count = last - first;
        const auto oldSize = m_size;

        resize_internal(oldSize + count,
            [it = first, &last](T* b, T* e) mutable
            {
                for (T* d = b; d != e; ++d, ++it)
                {
                    new (d) T{*it};
                }
            });

        return begin() + oldSize;
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

    template <typename T>
    void deque<T>::pop_back()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            back().~T();
        }

        --m_size;
    }

    template <typename T>
    T& deque<T>::front()
    {
        OBLO_ASSERT(m_size > 0);
        return at_unsafe(0);
    }

    template <typename T>
    const T& deque<T>::front() const
    {
        OBLO_ASSERT(m_size > 0);
        return at_unsafe(0);
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
        std::swap(m_start, other.m_start);
        std::swap(m_size, other.m_size);
        std::swap(m_elementsPerChunk, other.m_elementsPerChunk);
    }

    template <typename T>
    void deque<T>::shrink_to_fit() noexcept
    {
        // TODO: This does not get rid of the starting offset
        const usize requiredChunks = round_up_div(m_start + m_size, m_elementsPerChunk);

        if (requiredChunks != m_chunks.size())
        {
            shrink_internal(m_size);
            free_empty(requiredChunks);
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
            const usize firstDestroyedElement = m_start + newSize + 1;
            const usize firstChunk = firstDestroyedElement / m_elementsPerChunk;

            // Deal with the first chunk, where we might need to apply an offset
            {
                T* const chunk = m_chunks[firstChunk];

                const usize elementsInFirstChunk = (m_start + m_size) & (m_elementsPerChunk - 1);
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
    void deque<T>::grow_front(usize chunks)
    {
        const auto oldSize = m_chunks.size();
        m_chunks.resize_default(chunks + oldSize);
        rotate(m_chunks.begin(), m_chunks.begin() + oldSize, m_chunks.end());

        allocator* const a = get_allocator();

        for (usize i = 0; i < chunks; ++i)
        {
            chunk const c = reinterpret_cast<chunk>(a->allocate(sizeof(T) * m_elementsPerChunk, alignof(T)));
            m_chunks[i] = c;
        }

        m_start += chunks * m_elementsPerChunk;
    }

    template <typename T>
    OBLO_FORCEINLINE T& deque<T>::at_unsafe(usize index)
    {
        const auto offset = m_start + index;
        T* const chunk = m_chunks[offset / m_elementsPerChunk];
        return chunk[offset & (m_elementsPerChunk - 1)];
    }

    template <typename T>
    OBLO_FORCEINLINE const T& deque<T>::at_unsafe(usize index) const
    {
        const auto offset = m_start + index;
        const T* const chunk = m_chunks[offset / m_elementsPerChunk];
        return chunk[offset & (m_elementsPerChunk - 1)];
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