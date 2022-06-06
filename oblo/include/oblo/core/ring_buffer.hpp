#pragma once

#include <concepts>
#include <cstddef>
#include <oblo/core/debug.hpp>

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
            std::swap(m_size, other.m_size);
            std::swap(m_firstUnused, other.m_firstUnused);
            std::swap(m_usedCount, other.m_usedCount);
        }

        ring_buffer(std::size_t size)
        {
            m_buffer = new T[size];
            m_size = size;
        }

        ring_buffer& operator=(const ring_buffer&) = delete;

        ring_buffer& operator=(ring_buffer&& other) noexcept
        {
            std::swap(m_buffer, other.m_buffer);
            std::swap(m_size, other.m_size);
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
            m_size = 0u;
            m_firstUnused = 0u;
            m_usedCount = 0u;
        }

        void grow(std::size_t newSize) requires std::is_nothrow_move_assignable_v<T>
        {
            if (newSize == m_size)
            {
                return;
            }

            auto* const newBuffer = new T[newSize];
            auto* outIt = newBuffer;

            for (std::size_t i = 0; i < m_size; ++i)
            {
                const auto oldestIndex = (m_firstUnused + i) % m_size;
                *outIt = std::move(m_buffer[oldestIndex]);
                ++outIt;
            }

            delete[] m_buffer;
            m_buffer = newBuffer;
            m_firstUnused = m_usedCount;
            m_size = newSize;
        }

        bool has_available(std::size_t count)
        {
            return count <= m_size - m_usedCount;
        }

        segmented_span fetch(std::size_t count)
        {
            OBLO_ASSERT(has_available(count));

            const auto availableFirstSegment = m_size - m_firstUnused;

            segmented_span result{};

            if (count <= availableFirstSegment)
            {
                result.firstSegmentBegin = m_buffer + m_firstUnused;
                result.firstSegmentEnd = result.firstSegmentBegin + count;
                m_firstUnused = (m_firstUnused + count) % m_size;
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

        segmented_span used_segments(std::size_t maxCount)
        {
            OBLO_ASSERT(maxCount <= m_usedCount);
            segmented_span result{};

            const auto available = available_count();
            const auto firstUnused = (m_firstUnused + available) % m_size;
            const auto lastUnused = firstUnused + maxCount;

            if (lastUnused < m_size)
            {
                result.firstSegmentBegin = m_buffer + firstUnused;
                result.firstSegmentEnd = m_buffer + lastUnused;
            }
            else
            {
                const auto lastUnusedSecondSegmentLength = lastUnused % m_size;
                result.firstSegmentBegin = m_buffer + firstUnused;
                result.firstSegmentEnd = m_buffer + m_size;
                result.secondSegmentBegin = m_buffer;
                result.secondSegmentEnd = m_buffer + lastUnusedSecondSegmentLength;
            }

            return result;
        }

        T* first_used()
        {
            OBLO_ASSERT(m_usedCount > 0);
            const auto firstUnused = (m_firstUnused + available_count()) % m_size;
            return m_buffer + firstUnused;
        }

        void release(std::size_t count)
        {
            OBLO_ASSERT(count < m_usedCount);
            m_usedCount -= count;
        }

        std::size_t size() const
        {
            return m_size;
        }

        std::size_t used_count() const
        {
            return m_usedCount;
        }

        std::size_t available_count() const
        {
            return m_size - m_usedCount;
        }

    private:
        T* m_buffer{nullptr};
        std::size_t m_size{0u};
        std::size_t m_firstUnused{0u};
        std::size_t m_usedCount{0u};
    };
}