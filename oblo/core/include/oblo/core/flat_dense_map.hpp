#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <span>
#include <utility>
#include <vector>

namespace oblo
{
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

    template <typename Key, typename Value, typename KeyExtractor = flat_key_extractor<Key>>
    class flat_dense_map
    {
    public:
        using key_type = Key;
        using value_type = Value;
        using extractor_type = KeyExtractor;

        template <typename... Args>
        auto emplace(Key key, Args&&... args) noexcept
        {
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
                return std::pair{m_denseValue.begin() + pointedIndex, false};
            }

            const auto newIndex = narrow_cast<u32>(m_denseKey.size());

            m_sparse[keyIndex] = newIndex;
            m_denseKey.emplace_back(key);
            m_denseValue.emplace_back(std::forward<Args>(args)...);

            return std::pair{m_denseValue.begin() + newIndex, true};
        }

        u32 erase(Key key)
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

            if (const auto lastElementIndex = m_denseKey.size() - 1; pointedIndex != m_denseKey.size() - 1)
            {
                using namespace std;
                const auto lastElementKey = KeyExtractor::extract_key(m_denseKey[lastElementIndex]);
                m_sparse[lastElementKey] = pointedIndex;
                swap(m_denseKey[pointedIndex], m_denseKey[lastElementIndex]);
                swap(m_denseValue[pointedIndex], m_denseValue[lastElementIndex]);
            }

            m_sparse[keyIndex] = Invalid;
            m_denseKey.pop_back();
            m_denseValue.pop_back();
            return 1;
        }

        Value* try_find(Key key)
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

            return &m_denseValue[pointedIndex];
        }

        const Value* try_find(Key key) const
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

            return &m_denseValue[pointedIndex];
        }

        std::span<const Key> keys() const
        {
            return m_denseKey;
        }

        std::span<const Value> values() const
        {
            return m_denseValue;
        }

        std::span<Value> values()
        {
            return m_denseValue;
        }

        void reserve_sparse(u32 size)
        {
            m_sparse.reserve(size);
        }

        void reserve_dense(u32 size)
        {
            m_denseKey.reserve(size);
            m_denseValue.reserve(size);
        }

        void clear()
        {
            m_sparse.clear();
            m_denseKey.clear();
            m_denseValue.clear();
        }

        usize size() const
        {
            return m_denseKey.size();
        }

        bool empty() const
        {
            return size() == 0;
        }

    private:
        bool is_key_matched_unchecked(u32 candidate, u32 index) const
        {
            const auto key = m_denseKey[index];
            const auto keyIndex = KeyExtractor::extract_key(key);
            return keyIndex != Invalid && keyIndex == candidate;
        }

    private:
        static constexpr u32 Invalid{KeyExtractor::invalid_key()};

    private:
        dynamic_array<u32> m_sparse;
        dynamic_array<Key> m_denseKey;
        dynamic_array<Value> m_denseValue;
    };
}