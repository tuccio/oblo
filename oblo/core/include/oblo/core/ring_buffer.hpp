#pragma once

#include <type_traits>

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename T>
    class ring_buffer
    {
    public:
        struct segmented_span
        {
            T* firstSegmentBegin;
            T* firstSegmentEnd;
            T* secondSegmentBegin;
            T* secondSegmentEnd;
        };

    public:
        ring_buffer() = default;
        ring_buffer(const ring_buffer&) = delete;

        ring_buffer(ring_buffer&& other) noexcept
        {
            std::swap(m_buffer, other.m_buffer);
            std::swap(m_capacity, other.m_capacity);
            std::swap(m_firstUnused, other.m_firstUnused);
            std::swap(m_usedCount, other.m_usedCount);
        }

        explicit ring_buffer(usize size)
        {
            m_buffer = new T[size];
            m_capacity = size;
        }

        ring_buffer& operator=(const ring_buffer&) = delete;

        ring_buffer& operator=(ring_buffer&& other) noexcept
        {
            std::swap(m_buffer, other.m_buffer);
            std::swap(m_capacity, other.m_capacity);
            std::swap(m_firstUnused, other.m_firstUnused);
            std::swap(m_usedCount, other.m_usedCount);
            return *this;
        }

        ~ring_buffer()
        {
            reset();
        }

        void reset()
        {
            if (m_buffer == nullptr)
            {
                return;
            }

            delete[] m_buffer;

            m_buffer = nullptr;
            m_capacity = 0u;
            m_firstUnused = 0u;
            m_usedCount = 0u;
        }

        void grow(usize newSize)
            requires std::is_nothrow_move_assignable_v<T>
        {
            if (newSize == m_capacity)
            {
                return;
            }

            auto* const newBuffer = new T[newSize];
            auto* outIt = newBuffer;

            for (usize i = 0; i < m_capacity; ++i)
            {
                const auto oldestIndex = (m_firstUnused + i) % m_capacity;
                *outIt = std::move(m_buffer[oldestIndex]);
                ++outIt;
            }

            delete[] m_buffer;
            m_buffer = newBuffer;
            m_firstUnused = m_usedCount;
            m_capacity = newSize;
        }

        bool has_available(usize count)
        {
            return count <= m_capacity - m_usedCount;
        }

        segmented_span fetch(usize count)
        {
            OBLO_ASSERT(has_available(count));

            const auto availableFirstSegment = m_capacity - m_firstUnused;

            segmented_span result{};

            if (count <= availableFirstSegment)
            {
                result.firstSegmentBegin = m_buffer + m_firstUnused;
                result.firstSegmentEnd = result.firstSegmentBegin + count;
                m_firstUnused = (m_firstUnused + count) % m_capacity;
            }
            else
            {
                const auto remaining = count - availableFirstSegment;
                result.firstSegmentBegin = m_buffer + m_firstUnused;
                result.firstSegmentEnd = result.firstSegmentBegin + availableFirstSegment;
                result.secondSegmentBegin = m_buffer;
                result.secondSegmentEnd = result.secondSegmentBegin + remaining;
                m_firstUnused = remaining;
            }

            m_usedCount += count;

            return result;
        }

        segmented_span used_segments()
        {
            return used_segments(m_usedCount);
        }

        segmented_span used_segments(usize maxCount)
        {
            OBLO_ASSERT(maxCount <= m_usedCount);
            segmented_span result{};

            const auto available = available_count();
            const auto firstUnused = (m_firstUnused + available) % m_capacity;
            const auto lastUnused = firstUnused + maxCount;

            if (lastUnused < m_capacity)
            {
                result.firstSegmentBegin = m_buffer + firstUnused;
                result.firstSegmentEnd = m_buffer + lastUnused;
            }
            else
            {
                const auto lastUnusedSecondSegmentLength = lastUnused % m_capacity;
                result.firstSegmentBegin = m_buffer + firstUnused;
                result.firstSegmentEnd = m_buffer + m_capacity;
                result.secondSegmentBegin = m_buffer;
                result.secondSegmentEnd = m_buffer + lastUnusedSecondSegmentLength;
            }

            return result;
        }

        T* first_used()
        {
            OBLO_ASSERT(m_usedCount > 0);
            const auto firstUnused = (m_firstUnused + available_count()) % m_capacity;
            return m_buffer + firstUnused;
        }

        void release(usize count)
        {
            OBLO_ASSERT(count <= m_usedCount);
            m_usedCount -= count;
        }

        usize capacity() const
        {
            return m_capacity;
        }

        usize used_count() const
        {
            return m_usedCount;
        }

        usize available_count() const
        {
            return m_capacity - m_usedCount;
        }

    private:
        T* m_buffer{nullptr};
        usize m_capacity{0u};
        usize m_firstUnused{0u};
        usize m_usedCount{0u};
    };
}