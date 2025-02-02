#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/math/power_of_two.hpp>

#include <concepts>
#include <cstddef>

namespace oblo
{
    template <typename Size>
    class ring_buffer_tracker
    {
    public:
        struct segment
        {
            Size begin;
            Size end;
        };

        struct segmented_span
        {
            segment segments[2];
        };

    public:
        ring_buffer_tracker() = default;
        ring_buffer_tracker(const ring_buffer_tracker&) = default;
        ring_buffer_tracker(ring_buffer_tracker&& other) = default;

        explicit ring_buffer_tracker(Size size)
        {
            m_size = size;
        }

        ring_buffer_tracker& operator=(const ring_buffer_tracker&) = default;
        ring_buffer_tracker& operator=(ring_buffer_tracker&& other) noexcept = default;

        ~ring_buffer_tracker() = default;

        void reset(Size size = 0u)
        {
            m_size = size;
            m_firstUnused = 0u;
            m_usedCount = 0u;
        }

        bool has_available(Size count)
        {
            return count <= m_size - m_usedCount;
        }

        segmented_span fetch(Size count)
        {
            OBLO_ASSERT(has_available(count));

            const auto availableFirstSegment = m_size - m_firstUnused;

            segmented_span result{};

            if (count <= availableFirstSegment)
            {
                result.segments[0] = {.begin = m_firstUnused, .end = m_firstUnused + count};
                m_firstUnused = (m_firstUnused + count) % m_size;
            }
            else
            {
                const auto remaining = count - availableFirstSegment;
                result.segments[0] = {.begin = m_firstUnused, .end = m_firstUnused + availableFirstSegment};
                result.segments[1] = {.begin = 0u, .end = remaining};
                m_firstUnused = remaining;
            }

            m_usedCount += count;

            return result;
        }

        segmented_span try_fetch_contiguous_aligned(Size count, Size alignment)
        {
            segmented_span result{};

            OBLO_ASSERT(is_power_of_two(alignment));
            const auto firstAligned = align_power_of_two(m_firstUnused, alignment);

            const auto newEnd = firstAligned + count;

            if (newEnd > m_size)
            {
                // Can't allocate on the first segment, try allocating on the second segment
                const auto availableSecondSegment = m_firstUnused - m_usedCount;

                // No need to align anymore, we start from 0
                if (availableSecondSegment >= count)
                {
                    const auto availableFirstSegment = m_size - m_firstUnused;

                    m_firstUnused = count;
                    m_usedCount += availableFirstSegment + count;

                    result.segments[0] = {.begin = 0u, .end = count};
                }
            }
            else
            {
                result.segments[0] = {.begin = firstAligned, .end = newEnd};

                const auto alignedCount = newEnd - m_firstUnused;
                m_firstUnused = newEnd % m_size;
                m_usedCount += alignedCount;
            }

            OBLO_ASSERT(result.segments[0].begin % alignment == 0);

            return result;
        }

        void release(Size count)
        {
            OBLO_ASSERT(count <= m_usedCount);
            m_usedCount -= count;
        }

        Size size() const
        {
            return m_size;
        }

        Size used_count() const
        {
            return m_usedCount;
        }

        Size available_count() const
        {
            return m_size - m_usedCount;
        }

    private:
        Size m_size{0u};
        Size m_firstUnused{0u};
        Size m_usedCount{0u};
    };
}