#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/pair.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <iterator>
#include <new>
#include <utility>

namespace oblo
{
    template <typename Key, typename Value, typename Hash = hash<Key>, typename KeyEqual = std::equal_to<Key>>
    class flat_hash_map
    {
        static constexpr usize min_capacity = 8;

        // Has some state and a few bits of the hash cheaply rejecting
        using ctrl_type = u8;

        // ctrl byte encoding:
        //   empty = 0x00
        //   deleted = 0x01
        //   occupied = (hash & 0x7F) | 0x80 -> always in [0x80, 0xFF]
        static constexpr u8 ctrl_empty = 0x00;
        static constexpr u8 ctrl_deleted = 0x01;

    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = pair<const Key, Value>;
        using size_type = usize;
        using hasher = Hash;
        using key_equal = KeyEqual;

        class iterator;
        class const_iterator;

    public:
        flat_hash_map() : m_allocator{get_global_allocator()} {}

        explicit flat_hash_map(allocator* allocator) : m_allocator{allocator} {}

        flat_hash_map(const flat_hash_map& other) : m_allocator{other.m_allocator}
        {
            if (other.m_capacity != 0)
            {
                allocate_storage(other.m_capacity);

                for (usize i = 0; i < other.m_capacity; ++i)
                {
                    const u8 c = other.m_ctrl[i];

                    if (c != ctrl_empty && c != ctrl_deleted)
                    {
                        new (m_keys.data() + i) Key{other.m_keys[i]};
                        new (m_values.data() + i) Value{other.m_values[i]};
                        m_ctrl[i] = c;
                        ++m_size;
                    }
                }
            }
        }

        flat_hash_map(flat_hash_map&& other) noexcept
        {
            m_allocator = other.m_allocator;
            m_storage = other.m_storage;
            m_keys = other.m_keys;
            m_values = other.m_values;
            m_ctrl = other.m_ctrl;
            m_size = other.m_size;
            m_tombstones = other.m_tombstones;
            m_capacity = other.m_capacity;

            other.m_storage = {};
            other.m_keys = {};
            other.m_values = {};
            other.m_ctrl = {};
            other.m_size = 0;
            other.m_tombstones = 0;
            other.m_capacity = 0;
        }

        flat_hash_map& operator=(const flat_hash_map& other)
        {
            if (this != &other)
            {
                flat_hash_map tmp{other};
                swap(tmp);
            }

            return *this;
        }

        flat_hash_map& operator=(flat_hash_map&& other) noexcept
        {
            if (this != &other)
            {
                flat_hash_map tmp{std::move(other)};
                swap(tmp);
            }

            return *this;
        }

        ~flat_hash_map()
        {
            destroy_and_free();
        }

        pair<iterator, bool> emplace(Key key, Value value)
        {
            if (m_capacity == 0 || needs_grow())
            {
                grow();
            }

            const hash_type h = Hash{}(key);
            const u8 want = ctrl_of(h);
            const usize mask = m_capacity - 1;

            constexpr usize sentinel = ~usize{};

            usize index = h & mask;
            usize firstDeleted = sentinel;

            while (true)
            {
                const u8 c = m_ctrl[index];

                if (c == ctrl_empty)
                {
                    break;
                }

                if (c == ctrl_deleted)
                {
                    if (firstDeleted == sentinel)
                    {
                        firstDeleted = index;
                    }
                }
                else if (c == want && KeyEqual{}(m_keys[index], key))
                {
                    return {iterator{this, index}, false};
                }

                index = (index + 1) & mask;
            }

            const usize insertIndex = firstDeleted != sentinel ? firstDeleted : index;

            new (m_keys.data() + insertIndex) Key{std::move(key)};
            new (m_values.data() + insertIndex) Value{std::move(value)};
            m_ctrl[insertIndex] = want;

            ++m_size;

            if (firstDeleted != sentinel)
            {
                --m_tombstones;
            }

            return {iterator{this, insertIndex}, true};
        }

        Value& operator[](const Key& key)
        {
            const auto it = find(key);

            if (it != end())
            {
                return it->second;
            }

            return emplace(Key{key}, Value{}).first->second;
        }

        usize erase(const Key& key)
        {
            if (m_capacity == 0)
            {
                return 0;
            }

            const hash_type h = Hash{}(key);
            const u8 want = ctrl_of(h);
            const usize mask = m_capacity - 1;

            usize index = h & mask;

            while (m_ctrl[index] != ctrl_empty)
            {
                const u8 c = m_ctrl[index];

                if (c == want && KeyEqual{}(m_keys[index], key))
                {
                    destroy_at(index);
                    m_ctrl[index] = ctrl_deleted;
                    ++m_tombstones;
                    --m_size;

                    if (m_tombstones > m_size)
                    {
                        rehash(m_capacity);
                    }

                    return 1;
                }

                index = (index + 1) & mask;
            }

            return 0;
        }

        iterator find(const Key& key)
        {
            return find_impl(key);
        }

        const_iterator find(const Key& key) const
        {
            return find_impl(key);
        }

        bool contains(const Key& key) const
        {
            return find_impl(key) != end();
        }

        Value& at(const Key& key)
        {
            const auto it = find(key);
            OBLO_ASSERT(it != end());
            return it->second;
        }

        const Value& at(const Key& key) const
        {
            const auto it = find(key);
            OBLO_ASSERT(it != end());
            return it->second;
        }

        void clear() noexcept
        {
            destroy_occupied();

            if (m_capacity != 0)
            {
                for (usize i = 0; i < m_capacity; ++i)
                {
                    m_ctrl[i] = ctrl_empty;
                }
            }

            m_size = 0;
            m_tombstones = 0;
        }

        // Ensures the map can hold at least the given number of elements without rehashing.
        void reserve(usize count)
        {
            const usize required = count * 4 / 3 + 1;

            if (required > m_capacity)
            {
                rehash(round_up_capacity(required));
            }
        }

        usize size() const noexcept
        {
            return m_size;
        }

        bool empty() const noexcept
        {
            return m_size == 0;
        }

        usize capacity() const noexcept
        {
            return m_capacity;
        }

        iterator begin()
        {
            usize i = 0;
            while (i < m_capacity && m_ctrl[i] == ctrl_empty)
            {
                ++i;
            }

            return iterator{this, i};
        }

        iterator end()
        {
            return iterator{this, m_capacity};
        }

        const_iterator begin() const
        {
            usize i = 0;
            while (i < m_capacity && m_ctrl[i] == ctrl_empty)
            {
                ++i;
            }

            return const_iterator{this, i};
        }

        const_iterator end() const
        {
            return const_iterator{this, m_capacity};
        }

        allocator* get_allocator() const noexcept
        {
            return m_allocator;
        }

        void swap(flat_hash_map& other) noexcept
        {
            using std::swap;
            swap(m_allocator, other.m_allocator);
            swap(m_storage, other.m_storage);
            swap(m_keys, other.m_keys);
            swap(m_values, other.m_values);
            swap(m_ctrl, other.m_ctrl);
            swap(m_size, other.m_size);
            swap(m_tombstones, other.m_tombstones);
            swap(m_capacity, other.m_capacity);
        }

    private:
        iterator find_impl(const Key& key)
        {
            if (m_capacity == 0)
            {
                return iterator{this, m_capacity};
            }

            const hash_type h = Hash{}(key);
            const u8 want = ctrl_of(h);
            const usize mask = m_capacity - 1;

            usize index = h & mask;

            while (m_ctrl[index] != ctrl_empty)
            {
                const u8 c = m_ctrl[index];

                if (c == want && KeyEqual{}(m_keys[index], key))
                {
                    return iterator{this, index};
                }

                index = (index + 1) & mask;
            }

            return iterator{this, m_capacity};
        }

        const_iterator find_impl(const Key& key) const
        {
            if (m_capacity == 0)
            {
                return const_iterator{this, m_capacity};
            }

            const hash_type h = Hash{}(key);
            const u8 want = ctrl_of(h);
            const usize mask = m_capacity - 1;

            usize index = h & mask;

            while (m_ctrl[index] != ctrl_empty)
            {
                const u8 c = m_ctrl[index];

                if (c == want && KeyEqual{}(m_keys[index], key))
                {
                    return const_iterator{this, index};
                }

                index = (index + 1) & mask;
            }

            return const_iterator{this, m_capacity};
        }

        static u8 ctrl_of(hash_type h)
        {
            return static_cast<u8>((h & 0x7Fu) | 0x80u);
        }

        bool is_occupied(usize i) const
        {
            const u8 c = m_ctrl[i];
            return c != ctrl_empty && c != ctrl_deleted;
        }

        bool needs_grow() const
        {
            return m_size + 1 > (m_capacity * 3) / 4;
        }

        void grow()
        {
            const usize newCapacity = m_capacity == 0 ? min_capacity : m_capacity * 2;
            rehash(newCapacity);
        }

        static usize round_up_capacity(usize n)
        {
            usize cap = min_capacity;
            while (cap < n)
            {
                cap *= 2;
            }

            return cap;
        }

        static usize align_up(usize v, usize alignment)
        {
            return (v + alignment - 1) & ~(alignment - 1);
        }

        constexpr usize storage_alignment() const
        {
            return alignof(Key) > alignof(Value) ? alignof(Key) : alignof(Value);
        }

        usize storage_size(usize capacity) const
        {
            const usize keySize = capacity * sizeof(Key);
            const usize valOff = align_up(keySize, alignof(Value));
            const usize valSize = capacity * sizeof(Value);
            const usize ctrlOff = valOff + valSize;

            return ctrlOff + capacity * sizeof(ctrl_type);
        }

        void allocate_storage(
            usize capacity, byte*& storage, span<Key>& keys, span<Value>& values, span<ctrl_type>& ctrl)
        {
            const usize total = storage_size(capacity);
            const usize alignment = storage_alignment();

            byte* const mem = m_allocator->allocate(total, alignment);

            Key* const k = reinterpret_cast<Key*>(mem);
            const usize keySize = capacity * sizeof(Key);
            const usize valOff = align_up(keySize, alignof(Value));
            Value* const v = reinterpret_cast<Value*>(mem + valOff);
            const usize ctrlOff = valOff + capacity * sizeof(Value);
            ctrl_type* const c = reinterpret_cast<ctrl_type*>(mem + ctrlOff);

            for (usize i = 0; i < capacity; ++i)
            {
                c[i] = ctrl_empty;
            }

            storage = mem;
            keys = span<Key>{k, capacity};
            values = span<Value>{v, capacity};
            ctrl = span<ctrl_type>{c, capacity};
        }

        void allocate_storage(usize capacity)
        {
            allocate_storage(capacity, m_storage, m_keys, m_values, m_ctrl);
            m_capacity = capacity;
        }

        void destroy_at(usize i)
        {
            m_keys[i].~Key();
            m_values[i].~Value();
        }

        void destroy_occupied() noexcept
        {
            if (m_capacity == 0)
            {
                return;
            }

            for (usize i = 0; i < m_capacity; ++i)
            {
                if (is_occupied(i))
                {
                    destroy_at(i);
                }
            }
        }

        void destroy_and_free() noexcept
        {
            destroy_occupied();

            if (m_storage)
            {
                m_allocator->deallocate(m_storage, storage_size(m_capacity), storage_alignment());
                m_storage = {};
            }
        }

        void rehash(usize newCapacity)
        {
            byte* newStorage{};
            span<Key> newKeys;
            span<Value> newValues;
            span<ctrl_type> newCtrl;

            allocate_storage(newCapacity, newStorage, newKeys, newValues, newCtrl);

            const usize mask = newCapacity - 1;

            for (usize i = 0; i < m_capacity; ++i)
            {
                if (!is_occupied(i))
                {
                    continue;
                }

                const hash_type h = Hash{}(m_keys[i]);
                usize idx = h & mask;

                while (newCtrl[idx] != ctrl_empty)
                {
                    idx = (idx + 1) & mask;
                }

                new (newKeys.data() + idx) Key{std::move(m_keys[i])};
                new (newValues.data() + idx) Value{std::move(m_values[i])};
                newCtrl[idx] = ctrl_of(h);
            }

            destroy_and_free();

            m_storage = newStorage;
            m_keys = newKeys;
            m_values = newValues;
            m_ctrl = newCtrl;
            m_capacity = newCapacity;
            m_tombstones = 0;
        }

    private:
        allocator* m_allocator{};
        byte* m_storage{};
        span<Key> m_keys{};
        span<Value> m_values{};
        span<ctrl_type> m_ctrl{};
        usize m_size{};
        usize m_tombstones{};
        usize m_capacity{};

        inline static Key s_dummyKey{};
        inline static Value s_dummyValue{};
    };

    template <typename Key, typename Value, typename Hash, typename KeyEqual>
    class flat_hash_map<Key, Value, Hash, KeyEqual>::iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = flat_hash_map::value_type;
        using difference_type = ptrdiff;
        using reference = value_type&;
        using pointer = value_type*;

        struct deref_proxy
        {
            const Key& first;
            Value& second;
        };

        iterator(flat_hash_map* map, usize index) :
            m_map{map}, m_index{index}, m_deref{index < map->m_capacity ? map->m_keys[index] : s_dummyKey,
                                            index < map->m_capacity ? map->m_values[index] : s_dummyValue}
        {
        }

        iterator& operator++()
        {
            do
            {
                ++m_index;
            } while (m_index < m_map->m_capacity && m_map->m_ctrl[m_index] == flat_hash_map::ctrl_empty);

            refresh();

            return *this;
        }

        iterator operator++(int)
        {
            const auto tmp = *this;
            ++*this;
            return tmp;
        }

        deref_proxy* operator->()
        {
            return &m_deref;
        }

        const deref_proxy* operator->() const
        {
            return &m_deref;
        }

        deref_proxy& operator*()
        {
            return m_deref;
        }

        const deref_proxy& operator*() const
        {
            return m_deref;
        }

        bool operator==(const iterator& other) const
        {
            return m_map == other.m_map && m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        void refresh()
        {
            if (m_index < m_map->m_capacity)
            {
                new (&m_deref) deref_proxy{m_map->m_keys[m_index], m_map->m_values[m_index]};
            }
            else
            {
                new (&m_deref) deref_proxy{s_dummyKey, s_dummyValue};
            }
        }

        flat_hash_map* m_map;
        usize m_index;
        deref_proxy m_deref;

        friend class flat_hash_map;
    };

    template <typename Key, typename Value, typename Hash, typename KeyEqual>
    class flat_hash_map<Key, Value, Hash, KeyEqual>::const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = flat_hash_map::value_type;
        using difference_type = ptrdiff;
        using reference = const value_type&;
        using pointer = const value_type*;

        struct deref_proxy
        {
            const Key& first;
            const Value& second;
        };

        const_iterator(const flat_hash_map* map, usize index) :
            m_map{map}, m_index{index}, m_deref{index < map->m_capacity ? map->m_keys[index] : s_dummyKey,
                                            index < map->m_capacity ? map->m_values[index] : s_dummyValue}
        {
        }

        const_iterator& operator++()
        {
            do
            {
                ++m_index;
            } while (m_index < m_map->m_capacity && m_map->m_ctrl[m_index] == flat_hash_map::ctrl_empty);

            refresh();

            return *this;
        }

        const_iterator operator++(int)
        {
            const auto tmp = *this;
            ++*this;
            return tmp;
        }

        const deref_proxy* operator->() const
        {
            return &m_deref;
        }

        const deref_proxy& operator*() const
        {
            return m_deref;
        }

        bool operator==(const const_iterator& other) const
        {
            return m_map == other.m_map && m_index == other.m_index;
        }

        bool operator!=(const const_iterator& other) const
        {
            return !(*this == other);
        }

    private:
        void refresh()
        {
            if (m_index < m_map->m_capacity)
            {
                new (&m_deref) deref_proxy{m_map->m_keys[m_index], m_map->m_values[m_index]};
            }
            else
            {
                new (&m_deref) deref_proxy{s_dummyKey, s_dummyValue};
            }
        }

        const flat_hash_map* m_map;
        usize m_index;
        deref_proxy m_deref;

        friend class flat_hash_map;
    };
}
