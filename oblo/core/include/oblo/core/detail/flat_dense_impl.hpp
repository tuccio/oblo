#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <span>
#include <utility>

namespace oblo
{
    namespace detail
    {
        template <typename Key, typename Value, typename ValueStorage, typename KeyExtractor>
        class flat_dense_impl : ValueStorage
        {
            static constexpr bool has_value = !std::is_same_v<Value, std::nullptr_t>;

        public:
            using key_type = Key;
            using extractor_type = KeyExtractor;
            using value_type = Value;

            flat_dense_impl() = default;
            flat_dense_impl(const flat_dense_impl&) = default;
            flat_dense_impl(flat_dense_impl&&) noexcept = default;

            flat_dense_impl(allocator* allocator) :
                ValueStorage{allocator}, m_sparse{allocator}, m_denseKey{allocator} {};

            flat_dense_impl& operator=(const flat_dense_impl&) = default;
            flat_dense_impl& operator=(flat_dense_impl&&) noexcept = default;

            ~flat_dense_impl() = default;

            template <typename... Args>
            auto emplace(Key key, [[maybe_unused]] Args&&... args) noexcept
            {
                static_assert(has_value || sizeof...(args) == 0, "No arguments allowed for sets");

                const auto keyIndex = KeyExtractor::extract_key(key);
                OBLO_ASSERT(keyIndex != Invalid);

                if (keyIndex >= m_sparse.size())
                {
                    const u32 newSize = keyIndex + 1;

                    // Resize will cause the allocation to be exact, which might cause a lot of reallocations
                    m_sparse.reserve_exponential(newSize);
                    m_sparse.resize(newSize, Invalid);
                }
                else if (const auto pointedIndex = m_sparse[keyIndex];
                    pointedIndex < m_denseKey.size() && is_key_matched_unchecked(keyIndex, pointedIndex))
                {
                    return std::pair{dense_begin() + pointedIndex, false};
                }

                const auto newIndex = narrow_cast<u32>(m_denseKey.size());

                m_sparse[keyIndex] = newIndex;
                m_denseKey.emplace_back(key);

                if constexpr (has_value)
                {
                    ValueStorage::emplace_back(std::forward<Args>(args)...);
                }

                return std::pair{dense_begin() + newIndex, true};
            }

            usize erase(Key key)
            {
                const auto keyIndex = KeyExtractor::extract_key(key);

                if (keyIndex >= m_sparse.size())
                {
                    return 0;
                }

                const auto pointedIndex = m_sparse[keyIndex];

                if (pointedIndex >= m_denseKey.size() || !is_key_matched_unchecked(keyIndex, pointedIndex))
                {
                    return 0;
                }

                if (const auto lastElementIndex = m_denseKey.size() - 1; pointedIndex != lastElementIndex)
                {
                    using namespace std;
                    const auto lastElementKey = KeyExtractor::extract_key(m_denseKey[lastElementIndex]);
                    m_sparse[lastElementKey] = pointedIndex;

                    auto& removedKey = m_denseKey[pointedIndex];
                    auto& lastKey = m_denseKey[lastElementIndex];

                    removedKey = std::move(lastKey);

                    if constexpr (has_value)
                    {
                        auto& removedValue = ValueStorage::operator[](pointedIndex);
                        auto& lastValue = ValueStorage::operator[](lastElementIndex);

                        removedValue.~Value();
                        new (&removedValue) Value{std::move(lastValue)};
                    }
                }

                m_sparse[keyIndex] = Invalid;
                m_denseKey.pop_back();

                if constexpr (has_value)
                {
                    ValueStorage::pop_back();
                }

                return 1;
            }

            std::conditional_t<has_value, Value*, const Key*> try_find(Key key)
            {
                const auto keyIndex = KeyExtractor::extract_key(key);

                if (keyIndex >= m_sparse.size())
                {
                    return nullptr;
                }

                const auto pointedIndex = m_sparse[keyIndex];

                if (pointedIndex >= m_denseKey.size() || !is_key_matched_unchecked(keyIndex, pointedIndex))
                {
                    return nullptr;
                }

                return &*(dense_begin() + pointedIndex);
            }

            std::conditional_t<has_value, const Value*, const Key*> try_find(Key key) const
            {
                const auto keyIndex = KeyExtractor::extract_key(key);

                if (keyIndex >= m_sparse.size())
                {
                    return nullptr;
                }

                const auto pointedIndex = m_sparse[keyIndex];

                if (pointedIndex >= m_denseKey.size() || !is_key_matched_unchecked(keyIndex, pointedIndex))
                {
                    return nullptr;
                }

                return &*(dense_begin() + pointedIndex);
            }

            Value& at(Key key)
            {
                auto* const v = try_find(key);
                OBLO_ASSERT(v);
                return *v;
            }

            const Value& at(Key key) const
            {
                auto* const v = try_find(key);
                OBLO_ASSERT(v);
                return *v;
            }

            std::span<const Key> keys() const
            {
                return m_denseKey;
            }

            void reserve_sparse(usize size)
            {
                m_sparse.reserve(size);
            }

            void reserve_dense(usize size)
            {
                m_denseKey.reserve(size);

                if constexpr (has_value)
                {
                    ValueStorage::reserve(size);
                }
            }

            void clear()
            {
                m_sparse.clear();
                m_denseKey.clear();

                if constexpr (has_value)
                {
                    ValueStorage::clear();
                }
            }

            usize size() const
            {
                return m_denseKey.size();
            }

            u32 size32() const
            {
                return m_denseKey.size32();
            }

            bool empty() const
            {
                return size() == 0;
            }

        protected:
            const ValueStorage& value_storage() const
            {
                return static_cast<const ValueStorage&>(*this);
            }

            ValueStorage& value_storage()
            {
                return static_cast<ValueStorage&>(*this);
            }

        private:
            bool is_key_matched_unchecked(u32 candidate, u32 index) const
            {
                const auto key = m_denseKey[index];
                const auto keyIndex = KeyExtractor::extract_key(key);
                return keyIndex != Invalid && keyIndex == candidate;
            }

            auto dense_begin()
            {
                if constexpr (has_value)
                {
                    return ValueStorage::begin();
                }
                else
                {
                    return m_denseKey.cbegin();
                }
            }

            auto dense_begin() const
            {
                if constexpr (has_value)
                {
                    return ValueStorage::begin();
                }
                else
                {
                    return m_denseKey.cbegin();
                }
            }

        private:
            static constexpr u32 Invalid{KeyExtractor::invalid_key()};

        private:
            dynamic_array<u32> m_sparse;
            dynamic_array<Key> m_denseKey;
        };

        struct null_value_storage
        {
        };
    }

    template <typename T>
    struct flat_key_extractor
    {
        static constexpr u32 extract_key(const T& h) noexcept
        {
            return u32{h};
        }

        static consteval u32 invalid_key() noexcept
        {
            return ~u32{};
        }
    };
}